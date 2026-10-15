#pragma once
#include <tesla.hpp>

#define SECTION_SAME_SIZE(height) ((height - 73 - 80) / 2)
#define SECTION_BIG_SIZE(height) (((height - 73 - 80) / 3) * 2)
#define SECTION_SMALL_SIZE(height) ((height - 73 - 80) / 3)

using namespace tsl;
using namespace tsl::elm;

namespace ui {

    enum class SectionsLayout{
        same,           //both sections have the same height top 1/2 : bottom 1/2
        big_top,        //top section is higher top 2/3 : bottom 1/3
        big_bottom      //bottom section is higher top 1/3 : bottom 2/3
    };

    namespace elm {
        class CustomCategoryHeader : public Element {
        public:
            CustomCategoryHeader(const std::string &title, bool hasSeparator = false, bool alwaysSmall = false) : Element(), m_text(title), m_hasSeparator(hasSeparator), m_alwaysSmall(alwaysSmall) {
                // Not a list item: libultrahand's List skips it when navigating
                m_isItem = false;
            }
            virtual ~CustomCategoryHeader() {}

            virtual void draw(tsl::gfx::Renderer *renderer) override {
                renderer->drawRect(this->getX() - 2, this->getBottomBound() - 30, 5, 23, a(tsl::headerSeparatorColor));
                renderer->drawString(this->m_text, false, this->getX() + 13, this->getBottomBound() - 12, 15, tsl::headerTextColor);

                if (this->m_hasSeparator)
                    renderer->drawRect(this->getX(), this->getBottomBound(), this->getWidth(), 1, a(tsl::separatorColor));
            }

            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                // Check if the CategoryHeader is part of a list and if it's the first entry in it, half it's height
                if (List *list = dynamic_cast<List*>(this->getParent()); list != nullptr) {
                    if (list->getIndexInList(this) == 0 || m_alwaysSmall) {
                        this->setBoundaries(this->getX(), this->getY(), this->getWidth(), tsl::style::ListItemDefaultHeight / 2);
                        return;
                    }
                }

                this->setBoundaries(this->getX(), this->getY(), this->getWidth(), tsl::style::ListItemDefaultHeight);
            }

            virtual bool onClick(u64 keys) override {
                return false;
            }

            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                return nullptr;
            }

        private:
            std::string m_text;
            bool m_hasSeparator;
            bool m_alwaysSmall;
        };

        /**
         * @brief A item that goes into a list
         *
         */
        class SmallListItem : public Element {
        public:
            /**
             * @brief Constructor
             *
             * @param text Initial description text
             * @param value Initial value text
             * @param focusable Whether the item can be navigated to (status rows can't, but still react to touch)
             */
            SmallListItem(const std::string& text, const std::string& value = "", bool focusable = false)
                : Element(), m_text(text), m_value(value) {
                // libultrahand's List only navigates to items, and pads the top of lists whose first entry is one
                m_isItem = focusable;
            }
            virtual ~SmallListItem() {}

            virtual void draw(gfx::Renderer *renderer) override {
                if (this->m_touched && Element::getInputMode() == InputMode::Touch) {
                    renderer->drawRect(ELEMENT_BOUNDS(this), aWithOpacity(tsl::clickColor));
                }

                if (this->m_maxWidth == 0) {
                    if (this->m_value.length() > 0) {
                        auto [valueWidth, valueHeight] = renderer->getTextDimensions(this->m_value, false, 15);
                        this->m_maxWidth = this->getWidth() - valueWidth - 70;
                    } else {
                        this->m_maxWidth = this->getWidth() - 40;
                    }

                    auto [width, height] = renderer->getTextDimensions(this->m_text, false, 15);
                    this->m_trunctuated = static_cast<u32>(width) > this->m_maxWidth;

                    if (this->m_trunctuated) {
                        this->m_scrollText = this->m_text + "        ";
                        auto [width, height] = renderer->getTextDimensions(this->m_scrollText, false, 15);
                        this->m_scrollText += this->m_text;
                        this->m_textWidth = width;
                        this->m_ellipsisText = renderer->limitStringLength(this->m_text, false, 15, this->m_maxWidth);
                    } else {
                        this->m_textWidth = width;
                    }
                }

                const auto textColor = (this->m_focused && ult::useSelectionText) ? tsl::selectedTextColor : tsl::defaultTextColor;
                if (this->m_trunctuated) {
                    if (this->m_focused) {
                        renderer->enableScissoring(this->getX(), this->getY(), this->m_maxWidth + 40, this->getHeight());
                        renderer->drawString(this->m_scrollText, false, this->getX() + 20 - this->m_scrollOffset, this->getY() + 25, 15, textColor);
                        renderer->disableScissoring();
                        if (this->m_scrollAnimationCounter == 90) {
                            if (this->m_scrollOffset == this->m_textWidth) {
                                this->m_scrollOffset = 0;
                                this->m_scrollAnimationCounter = 0;
                            } else {
                                this->m_scrollOffset++;
                            }
                        } else {
                            this->m_scrollAnimationCounter++;
                        }
                    } else {
                        renderer->drawString(this->m_ellipsisText, false, this->getX() + 20, this->getY() + 25, 15, textColor);
                    }
                } else {
                    renderer->drawString(this->m_text, false, this->getX() + 20, this->getY() + 25, 15, textColor);
                }

                const auto valueColor = (this->m_focused && ult::useSelectionValue) ? tsl::selectedValueTextColor : this->m_color;
                renderer->drawString(this->m_value, false, this->getX() + this->m_maxWidth + 45, this->getY() + 25, 15, valueColor);
            }

            // libultrahand draws separators before the focus highlight, so the highlight covers them
            virtual void drawSeparators(gfx::Renderer *renderer) override {
                renderer->drawRect(this->getX(), this->getY(), this->getWidth(), 1, a(tsl::separatorColor));
            }

            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(this->getX(), this->getY(), this->getWidth(), tsl::style::ListItemDefaultHeight / 2);
            }

            virtual bool onClick(u64 keys) override {
                if (keys & HidNpadButton_A) {
                    this->playClickFeedback();
                    this->triggerClickAnimation();
                }
                else if (keys & (HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight))
                    this->m_clickAnimationProgress = 0;

                return Element::onClick(keys);
            }

            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) override {
                if (event == TouchEvent::Touch)
                    this->m_touched = this->isTouchInside(currX, currY);

                // Like libultrahand's own list items, sliding off the item cancels the tap
                if (event == TouchEvent::Hold && this->m_touched && !this->isTouchInside(currX, currY))
                    this->m_touched = false;

                if (event == TouchEvent::Release && this->m_touched) {
                    this->m_touched = false;

                    if (Element::getInputMode() == InputMode::Touch) {
                        // Like libultrahand's own list items, a tap moves the focus to the item
                        if (this->m_isItem) {
                            tsl::shiftItemFocus(this);
                            // shiftItemFocus() doesn't tell the list, which navigates and restores focus from its own index
                            if (auto list = dynamic_cast<List*>(this->getParent()); list != nullptr) {
                                if (const auto index = list->getIndexInList(this); index >= 0)
                                    list->setFocusedIndex(index);
                            }
                        }

                        bool handled = this->onClick(HidNpadButton_A);

                        this->m_clickAnimationProgress = 0;
                        return handled;
                    }
                }


                return false;
            }

            virtual void setFocused(bool state) override {
                this->m_scroll = false;
                this->m_scrollOffset = 0;
                this->m_scrollAnimationCounter = 0;
                Element::setFocused(state);
            }

            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                return this->m_isItem ? this : nullptr;
            }

            /**
             * @brief Sets the left hand description text of the list item
             *
             * @param text Text
             */
            inline void setText(const std::string& text) {
                this->m_text = text;
                this->m_scrollText = "";
                this->m_ellipsisText = "";
                this->m_maxWidth = 0;
            }

            /**
             * @brief Sets the right hand value text of the list item
             *
             * @param value Text
             * @param color Color the text should be drawn, default is the theme's "on" color
             */
            virtual inline void setColoredValue(const std::string& value, Color color = tsl::onTextColor) {
                this->m_value = value;
                this->m_color = color;
                this->m_maxWidth = 0;
            }

            /**
             * @brief Sets the right hand value text of the list item
             *
             * @param value Text
             * @param faint Should the text be drawn in the theme's "on" or "off" color
             */
            virtual inline void setValue(const std::string& value, bool faint = false) {
                this->m_value = value;
                this->m_faint = faint;
                if(m_faint==true)
                    this->m_color = tsl::offTextColor;
                else
                    this->m_color = tsl::onTextColor;

                this->m_maxWidth = 0;
            }

            /**
             * @brief Gets the left hand description text of the list item
             *
             * @return Text
             */
            inline const std::string& getText() const {
                return this->m_text;
            }

            /**
             * @brief Gets the right hand value text of the list item
             *
             * @return Value
             */
            inline const std::string& getValue() {
                return this->m_value;
            }

            protected:
                // inBounds() includes the bottom edge, which is the next row's top edge: leave it out so a tap there hits only one row
                inline bool isTouchInside(s32 x, s32 y) {
                    return this->inBounds(x, y) && (y < this->getBottomBound());
                }

                // Sound/haptic feedback (if enabled in Ultrahand's settings) for activating the item; status rows stay silent
                virtual void playClickFeedback() {
                    if (this->m_isItem)
                        triggerEnterFeedback();
                }

                std::string m_text;
                std::string m_value = "";
                std::string m_scrollText = "";
                std::string m_ellipsisText = "";

                bool m_scroll = false;
                bool m_trunctuated = false;
                bool m_faint = false;

                Color m_color = tsl::onTextColor;

                bool m_touched = false;

                u16 m_maxScroll = 0;
                u16 m_scrollOffset = 0;
                u32 m_maxWidth = 0;
                u32 m_textWidth = 0;
                u16 m_scrollAnimationCounter = 0;
        };

        /**
         * @brief A toggleable list item that changes the state from On to Off when the A button gets pressed
         *
         */
        class SmallToggleListItem : public SmallListItem {
        public:
            /**
             * @brief Constructor
             *
             * @param text Initial description text
             * @param initialState Is the toggle set to On or Off initially
             * @param onValue Value drawn if the toggle is on
             * @param offValue Value drawn if the toggle is off
             */
            SmallToggleListItem(const std::string& text, bool initialState, const std::string& onValue = "On", const std::string& offValue = "Off")
                : SmallListItem(text), m_state(initialState), m_onValue(onValue), m_offValue(offValue) {

                this->setState(this->m_state);
            }

            virtual ~SmallToggleListItem() {}

            virtual bool onClick(u64 keys) override {
                if (keys & HidNpadButton_A) {
                    this->m_state = !this->m_state;

                    this->setState(this->m_state);
                    this->m_stateChangedListener(this->m_state);

                    return SmallListItem::onClick(keys);
                }

                return false;
            }

            /**
             * @brief Gets the current state of the toggle
             *
             * @return State
             */
            virtual inline bool getState() {
                return this->m_state;
            }

            /**
             * @brief Sets the current state of the toggle. Updates the Value
             *
             * @param state State
             */
            virtual void setState(bool state) {
                this->m_state = state;

                if (state)
                    this->setValue(this->m_onValue, false);
                else
                    this->setValue(this->m_offValue, true);
            }

            /**
             * @brief Adds a listener that gets called whenever the state of the toggle changes
             *
             * @param stateChangedListener Listener with the current state passed in as parameter
             */
            void setStateChangedListener(std::function<void(bool)> stateChangedListener) {
                this->m_stateChangedListener = stateChangedListener;
            }

        protected:
            void playClickFeedback() override {
                if (this->m_state)
                    triggerOnFeedback();
                else
                    triggerOffFeedback();
            }

            bool m_state = true;
            std::string m_onValue, m_offValue;

            std::function<void(bool)> m_stateChangedListener = [](bool){};
        };

        class DoubleSectionOverlayFrame : public Element {
        public:
            /**
             * @brief Constructor
             *
             * @param title Name of the Overlay drawn bolt at the top
             * @param subtitle Subtitle drawn bellow the title e.g version number
             * @param hasSeparators draw separators, default false
             */

            DoubleSectionOverlayFrame(const std::string& title, const std::string& subtitle, const SectionsLayout& layout = SectionsLayout::same ,const bool& hasSeparators = false) : Element(), m_title(title), m_subtitle(subtitle), m_layout(layout), m_hasSeparators(hasSeparators) {
                // libultrahand clips focus highlights to the area below the header
                ult::activeHeaderHeight = 80;
                ult::loadWallpaperFileWhenSafe();
                m_isItem = false;
            }
            virtual ~DoubleSectionOverlayFrame() {
                if (this->m_topSection != nullptr)
                    delete this->m_topSection;
                if (this->m_bottomSection != nullptr)
                    delete this->m_bottomSection;
            }

            virtual void draw(tsl::gfx::Renderer *renderer) override {
                if (!ult::limitedMemory && !ult::refreshWallpaper.load(std::memory_order_acquire) && !ult::wallpaperData.empty() && ult::correctFrameSize)
                    renderer->drawWallpaper();
                else
                    renderer->fillScreen(a(tsl::defaultBackgroundColor));

                renderer->drawString(this->m_title, false, 20, 50, 30, tsl::defaultOverlayColor);
                renderer->drawString(this->m_subtitle, false, 20, 70, 15, tsl::bannerVersionTextColor);

                if(m_hasSeparators){
                    renderer->drawRect(15, 79, tsl::cfg::FramebufferWidth - 30, 1, a(tsl::bottomSeparatorColor));
                    switch(m_layout){
                        case(SectionsLayout::same):
                            renderer->drawRect(15, 80 + SECTION_SAME_SIZE(tsl::cfg::FramebufferHeight), tsl::cfg::FramebufferWidth - 30, 1, a(tsl::bottomSeparatorColor));
                            break;
                        case(SectionsLayout::big_top):
                            renderer->drawRect(15, 80 + SECTION_BIG_SIZE(tsl::cfg::FramebufferHeight), tsl::cfg::FramebufferWidth - 30, 1, a(tsl::bottomSeparatorColor));
                            break;
                        case(SectionsLayout::big_bottom):
                            renderer->drawRect(15, 80 + SECTION_SMALL_SIZE(tsl::cfg::FramebufferHeight), tsl::cfg::FramebufferWidth - 30, 1, a(tsl::bottomSeparatorColor));
                            break;
                    }
                }

                renderer->drawRect(15, tsl::cfg::FramebufferHeight - 73, tsl::cfg::FramebufferWidth - 30, 1, a(tsl::bottomSeparatorColor));

                // Same footer as libultrahand's OverlayFrame, which also sizes its Back/OK touch targets
                const std::string back = "\uE0E1" + ult::GAP_2 + ult::BACK;
                const std::string ok = "\uE0E0" + ult::GAP_2 + ult::OK;
                tsl::updateFooterButtonWidths(renderer, back, ok, false);
                renderer->drawStringWithColoredSections(back + ult::GAP_1 + ok + ult::GAP_1, false, tsl::s_footerSpecialChars, 30, 693, 23, tsl::bottomTextColor, tsl::buttonColor);
                if (!selectIsUsingFocusedColor)
                    renderer->drawStringWithColoredSections(ok + ult::GAP_1, false, tsl::s_footerSpecialChars, 30 + ult::backWidth.load(std::memory_order_acquire), 693, 23, tsl::unfocusedColor, tsl::unfocusedColor);

                if (this->m_topSection != nullptr)
                    this->m_topSection->frame(renderer);
                if (this->m_bottomSection != nullptr)
                    this->m_bottomSection->frame(renderer);

                tsl::drawEdgeSeparator(renderer);
            }

            virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
                this->setBoundaries(parentX, parentY, parentWidth, parentHeight);

                if (this->m_topSection != nullptr) {
                    switch(m_layout){
                        case(SectionsLayout::same):
                            this->m_topSection->setBoundaries(parentX + 35, parentY + 80, parentWidth - 85, SECTION_SAME_SIZE(parentHeight));
                            break;
                        case(SectionsLayout::big_top):
                            this->m_topSection->setBoundaries(parentX + 35, parentY + 80, parentWidth - 85, SECTION_BIG_SIZE(parentHeight));
                            break;
                        case(SectionsLayout::big_bottom):
                            this->m_topSection->setBoundaries(parentX + 35, parentY + 80, parentWidth - 85, SECTION_SMALL_SIZE(parentHeight));
                            break;
                    }
                    this->m_topSection->invalidate();
                }
                if (this->m_bottomSection != nullptr) {
                    // libultrahand's List draws up to 8px above and 6px below its bounds (partially scrolled rows),
                    // so keep it far enough from the middle separator and the footer line not to draw over them
                    const s32 bottom_top = parentY + 80 + this->m_topSection->getHeight() + 8 + 1;
                    const s32 bottom_end = parentY + parentHeight - 73 - 6 - 1;
                    this->m_bottomSection->setBoundaries(parentX + 35, bottom_top, parentWidth - 85, bottom_end - bottom_top);
                    this->m_bottomSection->invalidate();
                }
            }

            virtual Element* requestFocus(Element *oldFocus, FocusDirection direction) override {
                // libultrahand turns lone L/R/ZL/ZR presses into jump/skip requests for the focused list.
                // Those buttons are emuiibo actions here, so drop the request and keep the current focus.
                const bool jump_requested = ::jumpToTop.exchange(false, std::memory_order_acq_rel)
                                          | ::jumpToBottom.exchange(false, std::memory_order_acq_rel)
                                          | ::skipUp.exchange(false, std::memory_order_acq_rel)
                                          | ::skipDown.exchange(false, std::memory_order_acq_rel);
                if (jump_requested && (oldFocus != nullptr)) {
                    return oldFocus;
                }

                // Only the bottom section is navigable, the top one just shows status.
                // libultrahand routes Up presses here instead of to the focused element's list, so this must not move into the top section.
                if (this->m_bottomSection != nullptr)
                    return this->m_bottomSection->requestFocus(oldFocus, direction);
                return nullptr;
            }

            virtual bool onTouch(TouchEvent event, s32 currX, s32 currY, s32 prevX, s32 prevY, s32 initialX, s32 initialY) override {
                // A touch belongs to the section it started in: later Hold/Release events go there even when outside its bounds,
                // so items/lists can clear their touch state once the finger is lifted or leaves them
                if (event == TouchEvent::Touch) {
                    this->m_touchedSection = nullptr;
                    if (this->m_bottomSection != nullptr && this->m_bottomSection->inBounds(currX, currY))
                        this->m_touchedSection = this->m_bottomSection;
                    else if (this->m_topSection != nullptr && this->m_topSection->inBounds(currX, currY))
                        this->m_touchedSection = this->m_topSection;
                }

                if (this->m_touchedSection == nullptr)
                    return false;

                const auto handled = this->m_touchedSection->onTouch(event, currX, currY, prevX, prevY, initialX, initialY);
                if (event == TouchEvent::Release)
                    this->m_touchedSection = nullptr;
                return handled;
            }

            /**
             * @brief Sets the content of the top section
             *
             * @param content Element
             */
            virtual void setTopSection(Element *content) final {
                if (this->m_topSection != nullptr)
                    delete this->m_topSection;

                this->m_topSection = content;

                if (content != nullptr) {
                    this->m_topSection->setParent(this);
                    this->invalidate();
                }
            }

            /**
             * @brief Sets the content of the bottom section
             *
             * @param content Element
             */
            virtual void setBottomSection(Element *content) final {
                if (this->m_bottomSection != nullptr)
                    delete this->m_bottomSection;

                this->m_bottomSection = content;

                if (content != nullptr) {
                    this->m_bottomSection->setParent(this);
                    this->invalidate();
                }
            }


            /**
             * @brief Changes the title of the menu
             *
             * @param title Title to change to
             */
            virtual void setTitle(const std::string &title) final {
                this->m_title = title;
            }

            /**
             * @brief Changes the subtitle of the menu
             *
             * @param title Subtitle to change to
             */
            virtual void setSubtitle(const std::string &subtitle) final {
                this->m_subtitle = subtitle;
            }

        protected:
            Element *m_topSection = nullptr;
            Element *m_bottomSection = nullptr;
            Element *m_touchedSection = nullptr;
            std::string m_title, m_subtitle;
            SectionsLayout m_layout;
            bool m_hasSeparators;
        };
    }

}
