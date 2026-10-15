
#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <ui/ui_TeslaExtras.hpp>
#include <emu/emu_Service.hpp>
#include <ui/ui_PngImage.hpp>
#include <tr/tr_Translation.hpp>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <sys/stat.h>

namespace {

    constexpr auto ActionKeyShowHelp = HidNpadButton_Plus;
    constexpr auto ActionKeyEnableEmulation = HidNpadButton_R;
    constexpr auto ActionKeyDisableEmulation = HidNpadButton_L;
    constexpr auto ActionKeyActivateItem = HidNpadButton_A;
    constexpr auto ActionKeyAddToFavorite = HidNpadButton_Y;
    constexpr auto ActionKeyRemoveFromFavorite = HidNpadButton_X;
    constexpr auto ActionKeyToogleConnectVirtualAmiibo = HidNpadButton_StickR;
    constexpr auto ActionKeyResetActiveVirtualAmiibo = HidNpadButton_Minus;
    constexpr auto ActionKeyEnableRandomUuid = HidNpadButton_ZR;
    constexpr auto ActionKeyDisableRandomUuid = HidNpadButton_ZL;
    
    inline std::string GetActionKeyGlyph(const u64 action_key) {
        for(const auto &info : ult::KEYS_INFO) {
            if(info.key == action_key) {
                return info.glyph;
            }
        }

        return "?";
    }

}

namespace {

    enum class InitializationStatus {
        Ok,
        TranslationsNotLoaded,
        EmuiiboNotPresent,
        OkVersionMismatch,
        EmuiiboServiceError,
        OtherServiceError
    };

    enum class Icon {
        Help,
        Reset,
        Favorite
    };

    static const std::unordered_map<Icon, std::string> IconGlyphTable = {
        { Icon::Help, "\uE142" },
        { Icon::Reset, "\uE098" },
        { Icon::Favorite, "\u2605" },
    };

    inline std::string GetIconGlyph(const Icon icon) {
        return IconGlyphTable.at(icon);
    }

}

namespace {

    constexpr u32 IconMargin = 5;

    inline u32 GetIconMaxWidth() {
        return (tsl::cfg::LayerWidth / 2) - 2 * IconMargin;
    }

    constexpr u32 IconMaxHeight = 100 - 2 * IconMargin;

}

namespace {

    // Defined in Makefile
    constexpr emu::Version ExpectedVersion = { VER_MAJOR, VER_MINOR, VER_MICRO, {} };

    constexpr auto FavoritesFile = "sdmc:/emuiibo/overlay/favorites.txt";

    InitializationStatus g_InitializationStatus;
    Result g_InitializationResult;

    inline bool IsInitializationOk() {
        return (g_InitializationStatus == InitializationStatus::Ok) || (g_InitializationStatus == InitializationStatus::OkVersionMismatch);
    }

    std::string g_VirtualAmiiboDirectory;
    emu::Version g_Version;
    std::string g_ActiveVirtualAmiiboPath;
    emu::VirtualAmiiboData g_ActiveVirtualAmiiboData;
    ui::PngImage g_VirtualAmiiboImage;
    std::vector<std::string> g_Favorites;
    // Folders are listed by reading only the amiibo names, unless the sysmodule is a different version that may lack that command
    bool g_ListWithFullParse = false;

    constexpr size_t MaxVirtualAmiiboAreaCount = 15;
    u32 g_VirtualAmiiboAreaCount = 0;
    u32 g_VirtualAmiiboCurrentAreaIndex = 0;
    emu::VirtualAmiiboAreaEntry g_VirtualAmiiboAreaEntries[MaxVirtualAmiiboAreaCount];
    std::string g_VirtualAmiiboAreaTitles[MaxVirtualAmiiboAreaCount];

    inline bool IsActiveVirtualAmiiboValid() {
        return !g_ActiveVirtualAmiiboPath.empty();
    }

    inline std::string MakeVersionString() {
        if(!IsInitializationOk()) {
            if(g_InitializationStatus == InitializationStatus::TranslationsNotLoaded) {
                return "Unable to load translations!";
            }
            else if(g_InitializationStatus == InitializationStatus::EmuiiboNotPresent) {
                return "EmuiiboNotPresent"_tr;
            }
            else if(g_InitializationStatus == InitializationStatus::EmuiiboServiceError) {
                std::stringstream strm;
                strm << "EmuiiboServiceError"_tr;
                strm << " (0x" << std::hex << std::uppercase << g_InitializationResult << ")";
                return strm.str();
            }
            else if(g_InitializationStatus == InitializationStatus::OtherServiceError) {
                std::stringstream strm;
                strm << "OtherServiceError"_tr;
                strm << " (0x" << std::hex << std::uppercase << g_InitializationResult << ")";
                return strm.str();
            }
        }
        else {
            std::stringstream strm;
            strm << "emuiibo v" << (int)g_Version.major << "." << (int)g_Version.minor << "." << (int)g_Version.micro;
            strm << " (" << (g_Version.dev_build ? "dev" : "release") << ")";
            if(g_InitializationStatus == InitializationStatus::OkVersionMismatch) {
                strm << " (outdated, expected v" << (int)ExpectedVersion.major << "." << (int)ExpectedVersion.minor << "." << (int)ExpectedVersion.micro << ")";
            }
            return strm.str();
        }

        return std::string("Unknown statius (...)");
    }

    inline emu::VirtualAmiiboStatus GetActiveVirtualAmiiboStatus() {
        if(IsActiveVirtualAmiiboValid()) {
            return emu::GetActiveVirtualAmiiboStatus();
        }
        else {
            return emu::VirtualAmiiboStatus::Invalid;
        }
    }

    void ToggleEmulationStatus() {
        switch(emu::GetEmulationStatus()) {
            case emu::EmulationStatus::On: {
                emu::SetEmulationStatus(emu::EmulationStatus::Off);
                break;
            }
            case emu::EmulationStatus::Off: {
                emu::SetEmulationStatus(emu::EmulationStatus::On);
                break;
            }
        }
    }

    void ToggleActiveVirtualAmiiboStatus() {
        switch(emu::GetActiveVirtualAmiiboStatus()) {
            case emu::VirtualAmiiboStatus::Connected: {
                emu::SetActiveVirtualAmiiboStatus(emu::VirtualAmiiboStatus::Disconnected);
                break;
            }
            case emu::VirtualAmiiboStatus::Disconnected: {
                emu::SetActiveVirtualAmiiboStatus(emu::VirtualAmiiboStatus::Connected);
                break;
            }
            default: {
                break;
            }
        }
    }

    void LoadActiveVirtualAmiibo() {
        char active_virtual_amiibo_path_str[FS_MAX_PATH] = {};
        emu::GetActiveVirtualAmiibo(&g_ActiveVirtualAmiiboData, active_virtual_amiibo_path_str, sizeof(active_virtual_amiibo_path_str));
        g_ActiveVirtualAmiiboPath.assign(active_virtual_amiibo_path_str);

        g_VirtualAmiiboImage.Reset();
        if(IsActiveVirtualAmiiboValid()) {
            emu::GetActiveVirtualAmiiboAreas(g_VirtualAmiiboAreaEntries, sizeof(g_VirtualAmiiboAreaEntries), &g_VirtualAmiiboAreaCount);

            // Control data includes the game icon (~144KB), so it is only allocated while reading the area titles
            // If that fails, the titles fall back to the IDs
            std::unique_ptr<NsApplicationControlData> control_data;
            if(g_VirtualAmiiboAreaCount > 0) {
                control_data.reset(new (std::nothrow) NsApplicationControlData);
            }

            for(u32 i = 0; i < g_VirtualAmiiboAreaCount; i++) {
                const auto program_id = g_VirtualAmiiboAreaEntries[i].program_id;
                const auto access_id = g_VirtualAmiiboAreaEntries[i].access_id;
                std::stringstream strm;
                strm << std::hex << std::uppercase << std::setfill('0') << "0x" << std::setw(0x8) << access_id << " (" << std::setw(0x10) << program_id << ")";
                g_VirtualAmiiboAreaTitles[i] = strm.str();

                if(control_data && R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, program_id, control_data.get(), sizeof(NsApplicationControlData), nullptr))) {
                    tsl::hlp::doWithSmSession([&]() {
                        NacpLanguageEntry *entry = nullptr;
                        nacpGetLanguageEntry(&control_data->nacp, &entry);
                        if(entry != nullptr) {
                            g_VirtualAmiiboAreaTitles[i] = entry->name;
                        }
                    });
                }
            }

            u32 cur_access_id;
            if(R_SUCCEEDED(emu::GetActiveVirtualAmiiboCurrentArea(&cur_access_id))) {
                for(u32 i = 0; i < g_VirtualAmiiboAreaCount; i++) {
                    if(g_VirtualAmiiboAreaEntries[i].access_id == cur_access_id) {
                        g_VirtualAmiiboCurrentAreaIndex = i;
                        break;
                    }
                }
            }

            g_VirtualAmiiboImage.Load(g_ActiveVirtualAmiiboPath + "/amiibo.png", GetIconMaxWidth(), IconMaxHeight);
        }
    }

    inline void SetActiveVirtualAmiibo(const std::string &path) {
        emu::SetActiveVirtualAmiibo(path.c_str(), path.size());
        LoadActiveVirtualAmiibo();
    }

    inline void ResetActiveVirtualAmiibo() {
        emu::ResetActiveVirtualAmiibo();
        LoadActiveVirtualAmiibo();
    }

    inline void AddFavorite(const std::string &path) {
        g_Favorites.push_back(path);
    }

    inline void RemoveFavorite(const std::string &path) {
        g_Favorites.erase(std::remove(g_Favorites.begin(), g_Favorites.end(), path), g_Favorites.end()); 
    }

    inline bool IsFavorite(const std::string &path) {
        return std::find(g_Favorites.begin(), g_Favorites.end(), path) != g_Favorites.end();
    }

    // libultrahand keeps sdmc mounted for the overlay's whole lifetime, so SD paths can be used directly
    // (tsl::hlp::doWithSDCardHandle would unmount it again afterwards)

    void LoadFavorites() {
        g_Favorites.clear();
        std::ifstream favs_file(FavoritesFile);
        std::string fav_path_str;
        while(std::getline(favs_file, fav_path_str)) {
            AddFavorite(fav_path_str);
        }
    }

    void SaveFavorites() {
        std::ofstream file(FavoritesFile, std::ofstream::out | std::ofstream::trunc);
        for(const auto &fav_path: g_Favorites) {
            file << fav_path << std::endl;
        }
    }

    inline std::string GetPathFileName(const std::string &path) {
        return path.substr(path.find_last_of("/") + 1);
    }

    std::vector<std::string> SplitPath(const std::string &path) {
        std::vector<std::string> items;
        std::stringstream ss(path);
        std::string item;
        while(std::getline(ss, item, '/')) {
            items.push_back(item);
        }
        return items;
    }

    inline std::string GetRelativePathTo(const std::string &ref_path, const std::string &in_path) {
        const auto ref_path_items = SplitPath(ref_path);
        const auto in_path_items = SplitPath(in_path);
        u32 i = 0;
        std::string rel_path;
        for(; i < ref_path_items.size(); i++) {
            const auto cur_ref_path_item = ref_path_items.at(i);
            const auto cur_in_path_item = in_path_items.at(i);
            if(cur_ref_path_item != cur_in_path_item) {
                rel_path += "../";
            }
        }
        
        for(u32 j = i; j < in_path_items.size(); j++) {
            rel_path += in_path_items.at(j) + '/';
        }
        if(!rel_path.empty()) {
            rel_path.pop_back();
        }
        return rel_path;
    }

    inline std::string GetBaseDirectory(const std::string &path) {
        return path.substr(0, path.find_last_of("/"));
    }

    // Names of the virtual amiibos in a folder, saved after the folder is first listed, since reading each amiibo from the SD card
    // takes a few milliseconds (seconds for large folders). Only amiibos are stored, so other folders are always checked again
    constexpr auto NameIndexDirectory = "sdmc:/emuiibo/overlay/cache";
    constexpr auto NameIndexHeader = "emuiibo-name-index-v1";

    // Amiibo path -> name
    using NameIndex = std::unordered_map<std::string, std::string>;

    std::string MakeNameIndexPath(const std::string &folder_path) {
        // FNV-1a, so that file names stay the same across builds
        u64 hash = 0xCBF29CE484222325;
        for(const auto c: folder_path) {
            hash ^= static_cast<u8>(c);
            hash *= 0x100000001B3;
        }

        char file_name[0x20] = {};
        snprintf(file_name, sizeof(file_name), "%016lX.txt", hash);
        return std::string(NameIndexDirectory) + "/" + file_name;
    }

    NameIndex LoadNameIndex(const std::string &folder_path) {
        NameIndex index;
        std::ifstream ifs(MakeNameIndexPath(folder_path));
        std::string line;
        if(!std::getline(ifs, line) || (line != NameIndexHeader)) {
            return index;
        }

        while(std::getline(ifs, line)) {
            const auto separator = line.find('\t');
            if((separator == std::string::npos) || (separator == 0) || (separator + 1 == line.length())) {
                continue;
            }
            index.emplace(line.substr(0, separator), line.substr(separator + 1));
        }
        return index;
    }

    void SaveNameIndex(const std::string &folder_path, const std::vector<std::pair<std::string, std::string>> &entries) {
        mkdir(NameIndexDirectory, 0777);

        // Written to a temporary file first, so that an interrupted write leaves no partial index behind
        const auto index_path = MakeNameIndexPath(folder_path);
        const auto tmp_index_path = index_path + ".tmp";
        {
            std::ofstream ofs(tmp_index_path, std::ios::trunc);
            ofs << NameIndexHeader << '\n';
            for(const auto &[path, name]: entries) {
                // Skip anything that would break the line format; those amiibos just get read every time
                if((path.find_first_of("\t\n") != std::string::npos) || (name.find_first_of("\t\n") != std::string::npos)) {
                    continue;
                }
                ofs << path << '\t' << name << '\n';
            }
            if(!ofs) {
                ofs.close();
                remove(tmp_index_path.c_str());
                return;
            }
        }

        remove(index_path.c_str());
        rename(tmp_index_path.c_str(), index_path.c_str());
    }

}

class GuiListElement: public ui::elm::SmallListItem {
    private:
        std::string path;
        std::function<void(GuiListElement&)> action_listener;

    public:
        GuiListElement(const std::string &path, const std::string &label, const std::string &value = "") : ui::elm::SmallListItem(label, value, true), path(path) {
            this->setClickListener([&] (u64 keys) {
                if(keys & ActionKeyActivateItem) {
                    this->action_listener(*this);
                    return true;
                }
                return false;
            });
        }

        // Used by tsl::elm::List::jumpToItem() to pick the initially focused element
        bool matchesJumpCriteria(const std::string &jump_text, const std::string &jump_value, bool exact_match) const override {
            return !this->path.empty() && (this->path == jump_text);
        }

        inline void SetActionListener(const std::function<void(GuiListElement&)> &listener) {
            this->action_listener = listener;
        }

        inline const std::string &GetPath() const {
            return this->path;
        }

        inline bool IsFavorite() const {
            return ::IsFavorite(this->path);
        }

        inline void AddFavorite() {
            if(this->CanBeFavorite()) {
                ::AddFavorite(this->path);
                this->Update();
            }
        }

        inline void RemoveFavorite() {
            ::RemoveFavorite(this->path);
            this->Update();
        }

        virtual bool CanBeFavorite() const {
            return false;
        }

        inline bool ContainsVirtualAmiiboPath() const {
            if(!IsActiveVirtualAmiiboValid()) {
                return false;
            }

            return g_ActiveVirtualAmiiboPath.find(this->path) == 0;
        }

        virtual void Update() {}
};

class VirtualListElement: public GuiListElement {
    public:
        VirtualListElement(const std::string &label, const std::string &icon_glyph = "") : GuiListElement("", label + (!icon_glyph.empty() ? " " + icon_glyph : ""), "..") {}
};

class ActionListElement: public GuiListElement {
    public:
        ActionListElement(const std::string &label, const std::string &icon_glyph = "") : GuiListElement("", label + (!icon_glyph.empty() ? " " + icon_glyph : ""), "") {}
};

class FolderListElement: public GuiListElement {
    private:
        void Update() override {
            const std::string value = "..";
            this->setValue(this->IsFavorite() ? GetIconGlyph(Icon::Favorite) + " " + value : value);
        }
    
    public:
        FolderListElement(const std::string &path) : GuiListElement(path, GetPathFileName(path)) {
            this->Update();
        }
};

class AmiiboListElement: public GuiListElement {
    private:
        bool CanBeFavorite() const override {
            return true;
        }

        void Update() override {
            const auto value = GetActionKeyGlyph(ActionKeyActivateItem);
            this->setValue(this->IsFavorite() ? GetIconGlyph(Icon::Favorite) + " " + value : value);
        }
    
    public:
        AmiiboListElement(const std::string &path, const std::string &name) : GuiListElement(path, name) {
            this->Update();
        }
};

class AmiiboIcons: public tsl::elm::Element {
    private:
        ui::PngImage cur_virtual_amiibo_image;
        // Compared against the amiibo directory, since the image only records its own path when it loads successfully
        std::string cur_virtual_amiibo_path;

    public:
        static constexpr float ErrorTextFontSize = 15;

        AmiiboIcons() {
            // Static content, not a list item libultrahand should navigate to
            this->m_isItem = false;
        }

        void SetCurrentAmiiboPath(const std::string &path) {
            if(path == this->cur_virtual_amiibo_path) {
                return;
            }
            this->cur_virtual_amiibo_path = path;
            if(path.empty()) {
                this->cur_virtual_amiibo_image.Reset();
            }
            else {
                this->cur_virtual_amiibo_image.Load(path + "/amiibo.png", GetIconMaxWidth(), IconMaxHeight);
            }
        }

    private:
        void DrawIcon(tsl::gfx::Renderer* renderer, const s32 x, const s32 y, const s32 w, const s32 h, const ui::PngImage &image) {
            const auto img_buf = image.GetRGBABuffer();
            if(img_buf != nullptr) {
                renderer->drawBitmap(x + IconMargin / 2 + w / 2 - image.GetWidth() / 2, y + IconMargin, image.GetWidth(), image.GetHeight(), img_buf);
            }
            else {
                renderer->drawString(image.GetErrorText(), false, x + IconMargin, y + h / 2, ErrorTextFontSize, tsl::defaultTextColor);
            }
        }

        void DrawCustom(tsl::gfx::Renderer* renderer, const s32 x, const s32 y, const s32 w, const s32 h) {
            renderer->drawRect(x + w / 2 - 1, y, 1, h - IconMargin, this->a(tsl::defaultTextColor));
            this->DrawIcon(renderer, x, y, w / 2, h, g_VirtualAmiiboImage);
            this->DrawIcon(renderer, x + w / 2, y, w / 2, h, this->cur_virtual_amiibo_image);
        }

        virtual void draw(gfx::Renderer* renderer) override {
            renderer->enableScissoring(ELEMENT_BOUNDS(this));
            this->DrawCustom(renderer, ELEMENT_BOUNDS(this));
            renderer->disableScissoring();
        }

        virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {}
};

class AmiiboGuiHelp : public tsl::Gui {
    public:
        virtual tsl::elm::Element* createUI() override {
            auto root_frame = new ui::elm::DoubleSectionOverlayFrame("Help"_tr, MakeVersionString(), ui::SectionsLayout::big_top, false);
            auto top_list = new tsl::elm::List();
            root_frame->setTopSection(top_list);

            top_list->addItem(new ui::elm::SmallListItem("Help"_tr, GetActionKeyGlyph(ActionKeyShowHelp)));
            top_list->addItem(new ui::elm::SmallListItem("EnableEmulation"_tr, GetActionKeyGlyph(ActionKeyEnableEmulation)));
            top_list->addItem(new ui::elm::SmallListItem("DisableEmulation"_tr, GetActionKeyGlyph(ActionKeyDisableEmulation)));
            top_list->addItem(new ui::elm::SmallListItem("ToogleConnectVirtualAmiibo"_tr, GetActionKeyGlyph(ActionKeyToogleConnectVirtualAmiibo)));
            top_list->addItem(new ui::elm::SmallListItem("SelectFolderVirtualAmiibo"_tr, GetActionKeyGlyph(ActionKeyActivateItem)));
            top_list->addItem(new ui::elm::SmallListItem("AddFavorite"_tr, GetActionKeyGlyph(ActionKeyAddToFavorite)));
            top_list->addItem(new ui::elm::SmallListItem("RemoveFavorite"_tr, GetActionKeyGlyph(ActionKeyRemoveFromFavorite)));
            top_list->addItem(new ui::elm::SmallListItem("ResetActiveVirtualAmiibo"_tr, GetActionKeyGlyph(ActionKeyResetActiveVirtualAmiibo)));
            top_list->addItem(new ui::elm::SmallListItem("EnableRandomUuid"_tr, GetActionKeyGlyph(ActionKeyEnableRandomUuid)));
            top_list->addItem(new ui::elm::SmallListItem("DisableRandomUuid"_tr, GetActionKeyGlyph(ActionKeyDisableRandomUuid)));

            return root_frame;
        }
};

class AmiiboGui : public tsl::Gui {
    public:
        enum class Kind {
            Root,
            Favorites,
            Folder
        };

    private:
        Kind kind;
        std::string base_path;
        ui::elm::DoubleSectionOverlayFrame *root_frame;
        ui::elm::SmallToggleListItem *emulation_toggle_item;
        ui::elm::SmallListItem *game_header;
        ui::elm::SmallListItem *amiibo_header;
        ui::elm::SmallListItem *area_header;
        ui::elm::SmallToggleListItem *random_uuid_toggle_item;
        AmiiboIcons* amiibo_icons;
        tsl::elm::List *top_list;
        tsl::elm::List *bottom_list;

    public:
        AmiiboGui(const Kind kind, const std::string &path) : kind(kind), base_path(path) {}

        virtual tsl::elm::Element *createUI() override {
            // View frame with 2 sections
            this->root_frame = new ui::elm::DoubleSectionOverlayFrame("emuiibo", MakeVersionString(), ui::SectionsLayout::same, true);

            // Top and bottom containers
            this->top_list = new tsl::elm::List();
            this->root_frame->setTopSection(this->top_list);
            this->bottom_list = new tsl::elm::List();
            this->root_frame->setBottomSection(this->bottom_list);

            if(!IsInitializationOk()) {
                return this->root_frame;
            }

            if(this->kind == Kind::Root) {
                // Zero-height first entry: libultrahand pads the top of a list whose first element is focusable
                this->bottom_list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer*, s32, s32, s32, s32) {}));
                this->bottom_list->addItem(createRootElement());
                this->bottom_list->addItem(createFavoritesElement());
                this->bottom_list->addItem(createResetElement());
                this->bottom_list->addItem(createHelpElement());
            }
            else {
                // Iterate base folder
                u32 virtual_amiibo_count = 0;

                std::vector<std::string> dir_paths;
                if(this->kind == Kind::Favorites) {
                    dir_paths = g_Favorites;
                }
                else if(this->kind == Kind::Folder) {
                    auto dir = opendir(this->base_path.c_str());
                    if(dir) {
                        while(true) {
                            auto entry = readdir(dir);
                            if(entry == nullptr) {
                                break;
                            }
                            if(entry->d_type & DT_DIR) {
                                const auto dir_path = this->base_path + "/" + entry->d_name;
                                dir_paths.push_back(dir_path);
                            }
                        }
                        closedir(dir);
                    }
                }

                std::sort(dir_paths.begin(), dir_paths.end());

                // Favorites are not indexed: they may point to folders that no longer exist, which listing the folder would not reveal
                const auto use_name_index = (this->kind == Kind::Folder) && !g_ListWithFullParse;
                NameIndex name_index;
                if(use_name_index) {
                    name_index = LoadNameIndex(this->base_path);
                }
                std::vector<std::pair<std::string, std::string>> indexed_names;
                size_t name_index_hits = 0;
                auto name_index_changed = false;

                std::vector<GuiListElement*> items;
                std::string initial_focus_path;
                for(const auto &dir_path: dir_paths) {
                    std::string name;
                    auto is_amiibo = false;
                    if(const auto it = name_index.find(dir_path); it != name_index.end()) {
                        name = it->second;
                        is_amiibo = true;
                        name_index_hits++;
                    }
                    else {
                        is_amiibo = TryGetVirtualAmiiboName(dir_path, name);
                        name_index_changed |= is_amiibo;
                    }

                    GuiListElement *new_item;
                    if(is_amiibo) {
                        new_item = this->createAmiiboElement(dir_path, name);
                        indexed_names.emplace_back(dir_path, name);
                        virtual_amiibo_count++;
                    }
                    else {
                        new_item = this->createFolderElement(dir_path);
                    }
    
                    items.push_back(new_item);
                    if(new_item->ContainsVirtualAmiiboPath()) {
                        initial_focus_path = dir_path;
                    }
                }

                // Rewritten when amiibos were added, or removed (index entries left unused)
                if(use_name_index && (name_index_changed || (name_index_hits != name_index.size()))) {
                    SaveNameIndex(this->base_path, indexed_names);
                }

                // Information about current folder
                // Added before the items: libultrahand pads the top of a list whose first element is focusable
                this->bottom_list->addItem(new ui::elm::CustomCategoryHeader("AvailableVirtualAmiibos"_tr + " '" + GetPathFileName(this->base_path) + "': " + std::to_string(virtual_amiibo_count), true, true));
                for(auto item: items) {
                    this->bottom_list->addItem(item);
                }

                if(!initial_focus_path.empty()) {
                    this->bottom_list->jumpToItem(initial_focus_path);
                }
            }

            // Emulation status
            this->emulation_toggle_item = new ui::elm::SmallToggleListItem("EmulationStatus"_tr + " " + GetActionKeyGlyph(ActionKeyDisableEmulation) + " " + GetActionKeyGlyph(ActionKeyEnableEmulation), false, "On"_tr, "Off"_tr);
            this->emulation_toggle_item->setClickListener([&](u64 keys) {
                if(keys & ActionKeyActivateItem) {
                    ToggleEmulationStatus();
                    return true;
                }
                else {
                    return false;
                }
            });
            this->top_list->addItem(this->emulation_toggle_item);

            // Current game status
            this->game_header = new ui::elm::SmallListItem("CurrentGameIntercepted"_tr);
            this->top_list->addItem(this->game_header);

            // Current amiibo
            this->amiibo_header = new ui::elm::SmallListItem("");
            this->top_list->addItem(this->amiibo_header);

            // Current amiibo random UUID status
            this->random_uuid_toggle_item = new ui::elm::SmallToggleListItem("RandomUuid"_tr + " " + GetActionKeyGlyph(ActionKeyDisableRandomUuid) + " " + GetActionKeyGlyph(ActionKeyEnableRandomUuid), false, "On"_tr, "Off"_tr);
            this->random_uuid_toggle_item->setClickListener([&](u64 keys) {
                if(keys & ActionKeyActivateItem) {
                    ToggleEmulationStatus();
                    return true;
                }
                else {
                    return false;
                }
            });
            this->top_list->addItem(this->random_uuid_toggle_item);

            // Current amiibo area
            this->area_header = new ui::elm::SmallListItem("");
            this->top_list->addItem(this->area_header);

            // Current amiibo icon
            this->amiibo_icons = new AmiiboIcons();
            this->top_list->addItem(this->amiibo_icons, IconMaxHeight + 2 * IconMargin);

            // Main key bindings (L/R/ZL/ZR are handled as taps in handleInput())
            this->root_frame->setClickListener([&](u64 keys) {
                if(keys & ActionKeyShowHelp) {
                    tsl::changeTo<AmiiboGuiHelp>();
                    return true;
                }
                if(keys & ActionKeyToogleConnectVirtualAmiibo) {
                    ToggleActiveVirtualAmiiboStatus();
                    return true;
                }
                if(keys & ActionKeyResetActiveVirtualAmiibo) {
                    ResetActiveVirtualAmiibo();
                    return true;
                }
                if(auto gui_item = dynamic_cast<GuiListElement*>(getFocusedElement())) {
                    if(keys & ActionKeyAddToFavorite) {
                        gui_item->AddFavorite();
                        return true;
                    }
                    else if(keys & ActionKeyRemoveFromFavorite) {
                        gui_item->RemoveFavorite();
                        return true;
                    }
                }
                return false;
            });

            update();

            return root_frame;
        }

        virtual void update() override {
            if(!IsInitializationOk()) {
                return;
            }

            const auto is_intercepted = emu::IsCurrentApplicationIdIntercepted();
            this->game_header->setColoredValue(is_intercepted ? "Intercepted"_tr : "NotIntercepted"_tr, is_intercepted ? tsl::onTextColor : tsl::warningTextColor);

            const auto has_active_virtual_amiibo = IsActiveVirtualAmiiboValid();
            if(has_active_virtual_amiibo) {
                this->amiibo_header->setText(std::string(g_ActiveVirtualAmiiboData.name) + " " + GetActionKeyGlyph(ActionKeyToogleConnectVirtualAmiibo));
            }
            else {
                this->amiibo_header->setText("NoActiveVirtualAmiibo"_tr);
            }

            const auto is_connected = GetActiveVirtualAmiiboStatus() == emu::VirtualAmiiboStatus::Connected;
            this->amiibo_header->setColoredValue(is_connected ? "Connected"_tr : "Disconnected"_tr, is_connected ? tsl::onTextColor : tsl::warningTextColor);

            if(auto amiibo_item = dynamic_cast<AmiiboListElement*>(getFocusedElement())) {
                this->amiibo_icons->SetCurrentAmiiboPath(amiibo_item->GetPath());
            }
            else {
                this->amiibo_icons->SetCurrentAmiiboPath("");
            }

            this->emulation_toggle_item->setState(emu::GetEmulationStatus() == emu::EmulationStatus::On);

            if(has_active_virtual_amiibo) {
                if(g_VirtualAmiiboAreaCount > 0) {
                    this->area_header->setText("SelectedArea"_tr + " (" + std::to_string(g_VirtualAmiiboCurrentAreaIndex + 1) + " / " + std::to_string(g_VirtualAmiiboAreaCount) + "): " + g_VirtualAmiiboAreaTitles[g_VirtualAmiiboCurrentAreaIndex]);
                }
                else {
                    this->area_header->setText("NoVirtualAmiiboAreas"_tr);
                }

                this->random_uuid_toggle_item->setState(g_ActiveVirtualAmiiboData.uuid_info.use_random_uuid);
            }
            else {
                this->area_header->setText("NoActiveVirtualAmiibo"_tr);
                this->random_uuid_toggle_item->setState(false);
            }

            tsl::Gui::update();
        }

        // L/R/ZL/ZR act on release, and only for a short press of that button alone, like libultrahand's own shoulder shortcuts.
        // This keeps them from firing as part of the overlay key combo (by default ZL + ZR + DPAD-down).
        // They are handled here rather than in the frame's click listener so they also work when nothing is focused
        virtual bool handleInput(u64 keys_down, u64 keys_held, const HidTouchState &touch_pos, HidAnalogStickState left_joystick, HidAnalogStickState right_joystick) override {
            struct ShoulderKeyState {
                u64 key;
                bool pressed;
                bool isolated;
                u64 press_start_ns;
            };
            // Shared between GUI instances, since a press can start on one GUI and end on the next
            static ShoulderKeyState shoulder_key_states[] = {
                { ActionKeyEnableEmulation },
                { ActionKeyDisableEmulation },
                { ActionKeyEnableRandomUuid },
                { ActionKeyDisableRandomUuid }
            };
            constexpr u64 MaxTapDurationNs = 400'000'000;

            const auto now_ns = ult::nowNs();
            auto handled = false;
            for(auto &state: shoulder_key_states) {
                const auto other_keys_held = (keys_held & ~state.key & ALL_KEYS_MASK) != 0;
                if(keys_held & state.key) {
                    // Only count presses that start while the overlay is visible, not keys still held from the combo that opened it
                    if(keys_down & state.key) {
                        state.pressed = true;
                        state.isolated = true;
                        state.press_start_ns = now_ns;
                    }
                    if(other_keys_held) {
                        state.isolated = false;
                    }
                }
                else {
                    if(state.pressed && state.isolated && !other_keys_held && ((now_ns - state.press_start_ns) < MaxTapDurationNs)) {
                        this->onShoulderKeyTap(state.key);
                        handled = true;
                    }
                    state.pressed = false;
                    state.isolated = false;
                }
            }

            return handled;
        }

    private:
        void onShoulderKeyTap(const u64 key) {
            switch(key) {
                case ActionKeyEnableEmulation: {
                    emu::SetEmulationStatus(emu::EmulationStatus::On);
                    break;
                }
                case ActionKeyDisableEmulation: {
                    emu::SetEmulationStatus(emu::EmulationStatus::Off);
                    break;
                }
                case ActionKeyEnableRandomUuid: {
                    g_ActiveVirtualAmiiboData.uuid_info.use_random_uuid = true;
                    emu::SetActiveVirtualAmiiboUuidInfo(g_ActiveVirtualAmiiboData.uuid_info);
                    break;
                }
                case ActionKeyDisableRandomUuid: {
                    g_ActiveVirtualAmiiboData.uuid_info.use_random_uuid = false;
                    emu::SetActiveVirtualAmiiboUuidInfo(g_ActiveVirtualAmiiboData.uuid_info);
                    break;
                }
            }
        }

        VirtualListElement* createRootElement() {
            auto item = new VirtualListElement("ViewVirtualAmiibos"_tr);
            item->SetActionListener([&] (auto&) {
                tsl::changeTo<AmiiboGui>(Kind::Folder, g_VirtualAmiiboDirectory);
                // When root gets selected for the first time and we have an active virtual amiibo, we start directly at the active virtual amiibo dir
                static bool is_first_time = true;
                if(is_first_time && IsActiveVirtualAmiiboValid()) {
                    const auto active_virtual_amiibo_rel_dir = GetBaseDirectory(GetRelativePathTo(g_VirtualAmiiboDirectory, g_ActiveVirtualAmiiboPath));
                    auto incremental_path = g_VirtualAmiiboDirectory;
                    for(const auto &dir_item: SplitPath(active_virtual_amiibo_rel_dir)) {
                        incremental_path += "/" + dir_item;
                        tsl::changeTo<AmiiboGui>(Kind::Folder, incremental_path);
                    }
                }
                is_first_time = false;
            });
            return item;
        }

        VirtualListElement* createFavoritesElement() {
            auto item = new VirtualListElement("ViewFavorites"_tr, GetIconGlyph(Icon::Favorite));
            item->SetActionListener([&](auto&) {
                tsl::changeTo<AmiiboGui>(Kind::Favorites, "<favorites>");
            });
            return item;
        }

        ActionListElement* createResetElement() {
            auto item = new ActionListElement("ResetActiveVirtualAmiibo"_tr, GetIconGlyph(Icon::Reset));
            item->SetActionListener([&](auto&) {
                ResetActiveVirtualAmiibo();
                update();
            });
            return item;
        }

        ActionListElement* createHelpElement() {
            auto item = new ActionListElement("Help"_tr, GetIconGlyph(Icon::Help));
            item->SetActionListener([&](auto&) {
                tsl::changeTo<AmiiboGuiHelp>();
            });
            return item;
        }

        FolderListElement* createFolderElement(const std::string &path) {
            auto item = new FolderListElement(path);
            item->SetActionListener([&](auto& caller) {
                tsl::changeTo<AmiiboGui>(Kind::Folder, caller.GetPath());
            });
            return item;
        }

        static bool TryGetVirtualAmiiboName(const std::string &path, std::string &out_name) {
            if(g_ListWithFullParse) {
                emu::VirtualAmiiboData data = {};
                if(R_FAILED(emu::TryParseVirtualAmiibo(path.c_str(), path.length(), &data))) {
                    return false;
                }
                out_name = data.name;
            }
            else {
                emu::VirtualAmiiboName amiibo_name = {};
                if(R_FAILED(emu::TryGetVirtualAmiiboName(path.c_str(), path.length(), &amiibo_name))) {
                    return false;
                }
                out_name = amiibo_name.name;
            }
            return true;
        }

        AmiiboListElement* createAmiiboElement(const std::string &path, const std::string &name) {
            auto item = new AmiiboListElement(path, name);
            item->SetActionListener([&](auto& caller) {
                const auto path = caller.GetPath();
                if(g_ActiveVirtualAmiiboPath != path) {
                    SetActiveVirtualAmiibo(path);
                }
                else {
                    ToggleActiveVirtualAmiiboStatus();
                }
            });
            return item;
        }
};

class EmuiiboOverlay : public tsl::Overlay {
    public:
        virtual void initServices() override {
            if(!tr::Load()) {
                g_InitializationStatus = InitializationStatus::TranslationsNotLoaded;
                return;
            }
            if(!emu::IsAvailable()) {
                g_InitializationStatus = InitializationStatus::EmuiiboNotPresent;
                return;
            }
            g_InitializationResult = emu::Initialize();
            if(R_FAILED(g_InitializationResult)) {
                g_InitializationStatus = InitializationStatus::EmuiiboServiceError;
                return;
            }
            g_InitializationResult = pmdmntInitialize();
            if(R_FAILED(g_InitializationResult)) {
                g_InitializationStatus = InitializationStatus::OtherServiceError;
                return;
            }
            g_InitializationResult = nsInitialize();
            if(R_FAILED(g_InitializationResult)) {
                g_InitializationStatus = InitializationStatus::OtherServiceError;
                return;
            }

            g_Version = emu::GetVersion();
            if(g_Version.EqualsExceptBuild(ExpectedVersion)) {
                g_InitializationStatus = InitializationStatus::Ok;
            }
            else {
                g_InitializationStatus = InitializationStatus::OkVersionMismatch;
            }

            g_ListWithFullParse = g_InitializationStatus == InitializationStatus::OkVersionMismatch;

            char virtual_amiibo_dir_str[FS_MAX_PATH] = {};
            emu::GetVirtualAmiiboDirectory(virtual_amiibo_dir_str, sizeof(virtual_amiibo_dir_str));
            g_VirtualAmiiboDirectory.assign(virtual_amiibo_dir_str);
        }

        virtual void exitServices() override {
            SaveFavorites();
            nsExit();
            pmdmntExit();
            emu::Exit();
        }

        virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
            LoadActiveVirtualAmiibo();
            LoadFavorites();
            return initially<AmiiboGui>(AmiiboGui::Kind::Root, "<root>");
        }
};

int main(int argc, char **argv) {
    return tsl::loop<EmuiiboOverlay, tsl::impl::LaunchFlags::CloseOnExit>(argc, argv);
}