//! Minimal native combat rules for Conflict.
//! Exported through a stable C ABI so the Win32 C++ host can load this DLL.

#[no_mangle]
pub extern "C" fn conflict_damage(base_damage: i32, is_blocking: i32) -> i32 {
    let damage = base_damage.max(0);
    if is_blocking != 0 {
        (damage / 3).max(1)
    } else {
        damage
    }
}

#[no_mangle]
pub extern "C" fn conflict_can_hit(distance: f32, reach: f32) -> i32 {
    if distance.is_finite() && reach.is_finite() && distance <= reach && distance >= 0.0 {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn conflict_core_version() -> u32 {
    0x0003_0000
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn blocking_reduces_damage() {
        assert_eq!(conflict_damage(14, 1), 4);
        assert_eq!(conflict_damage(14, 0), 14);
    }

    #[test]
    fn hit_range_is_checked() {
        assert_eq!(conflict_can_hit(80.0, 112.0), 1);
        assert_eq!(conflict_can_hit(140.0, 112.0), 0);
        assert_eq!(conflict_can_hit(f32::NAN, 112.0), 0);
    }
}
