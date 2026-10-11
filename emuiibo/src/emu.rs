use crate::amiibo;
use crate::fsext;
use alloc::vec::Vec;
use nx::ipc::sf::ncm;
use nx::sync;

use atomic_enum::atomic_enum;

use core::sync::atomic::Ordering;

#[derive(nx::ipc::sf::Request, nx::ipc::sf::Response, Copy, Clone)]
#[repr(C)]
pub struct Version {
    pub major: u8,
    pub minor: u8,
    pub micro: u8,
    pub is_dev_build: bool,
}

impl Version {
    pub const fn from(major: u8, minor: u8, micro: u8, is_dev_build: bool) -> Self {
        Self {
            major: major,
            minor: minor,
            micro: micro,
            is_dev_build: is_dev_build,
        }
    }
}

#[atomic_enum]
#[derive(nx::ipc::sf::Request, nx::ipc::sf::Response, PartialEq, Eq)]
#[repr(u32)]
pub enum EmulationStatus {
    On,
    Off,
}


#[atomic_enum]
#[derive(nx::ipc::sf::Request, nx::ipc::sf::Response, PartialEq, Eq)]
#[repr(u32)]
#[allow(dead_code)]
pub enum VirtualAmiiboStatus {
    Invalid,
    Connected,
    Disconnected,
}

#[cfg(debug_assertions)]
pub const IS_DEV_BUILD: bool = true;

#[cfg(not(debug_assertions))]
pub const IS_DEV_BUILD: bool = false;

pub const CURRENT_VERSION: Version = Version::from(1, 1, 5, IS_DEV_BUILD);

static G_EMULATION_STATUS: AtomicEmulationStatus = AtomicEmulationStatus::new(EmulationStatus::Off);
static G_ACTIVE_VIRTUAL_AMIIBO_STATUS: AtomicVirtualAmiiboStatus =
    AtomicVirtualAmiiboStatus::new(VirtualAmiiboStatus::Invalid);
static G_INTERCEPTED_APPLICATION_IDS: sync::Mutex<Vec<u64>> = sync::Mutex::new(Vec::new());
static G_ACTIVE_VIRTUAL_AMIIBO: sync::Mutex<Option<amiibo::fmt::VirtualAmiibo>> =
    sync::Mutex::new(None);

const STATUS_ON_FLAG: &str = "status_on";

pub fn get_emulation_status() -> EmulationStatus {
    G_EMULATION_STATUS.load(Ordering::SeqCst)
}

pub fn load_emulation_status() {
    let status = if fsext::has_flag(STATUS_ON_FLAG) {
        EmulationStatus::On
    } else {
        EmulationStatus::Off
    };

    G_EMULATION_STATUS.store(status, Ordering::SeqCst);
}

pub fn set_emulation_status(status: EmulationStatus) {
    G_EMULATION_STATUS.store(status, Ordering::SeqCst);
    fsext::set_flag(STATUS_ON_FLAG, status == EmulationStatus::On);
}

pub fn get_active_virtual_amiibo_status() -> VirtualAmiiboStatus {
    G_ACTIVE_VIRTUAL_AMIIBO_STATUS.load(Ordering::SeqCst)
}

pub fn set_active_virtual_amiibo_status(status: VirtualAmiiboStatus) {
    G_ACTIVE_VIRTUAL_AMIIBO_STATUS.store(status, Ordering::SeqCst);
}

pub fn register_intercepted_application_id(application_id: ncm::ProgramId) {
    log!("RegisterInterceptedApplicationId -- application_id: {:#X}\n", application_id.0);
    G_INTERCEPTED_APPLICATION_IDS.lock().push(application_id.0);
}

pub fn unregister_intercepted_application_id(application_id: ncm::ProgramId) {
    log!("UnregisterInterceptedApplicationId -- application_id: {:#X}\n", application_id.0);
    G_INTERCEPTED_APPLICATION_IDS
        .lock()
        .retain(|&id| id != application_id.0);
}

pub fn is_application_id_intercepted(application_id: ncm::ProgramId) -> bool {
    G_INTERCEPTED_APPLICATION_IDS
        .lock()
        .contains(&application_id.0)
}

pub fn get_active_virtual_amiibo<'a>() -> sync::MutexGuard<'a, Option<amiibo::fmt::VirtualAmiibo>> {
    G_ACTIVE_VIRTUAL_AMIIBO.lock()
}

pub fn set_active_virtual_amiibo(virtual_amiibo: Option<amiibo::fmt::VirtualAmiibo>) {
    G_ACTIVE_VIRTUAL_AMIIBO_STATUS.store(
        if virtual_amiibo.is_some() {
            VirtualAmiiboStatus::Connected
        } else {
            VirtualAmiiboStatus::Invalid
        },
        Ordering::SeqCst,
    );
    *G_ACTIVE_VIRTUAL_AMIIBO.lock() = virtual_amiibo;
}
