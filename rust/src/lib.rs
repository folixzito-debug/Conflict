//! Real-time unit simulation for Conflict 0.6.0.
use std::sync::Mutex;

const UNITS_PER_TEAM: usize = 20;
const TOTAL_UNITS: usize = UNITS_PER_TEAM * 2;
const RESET_DELAY: f32 = 2.6;

const SWORDSMAN: u32 = 0;
const MEDIC: u32 = 1;
const CATAPULT: u32 = 2;
const MODERN: u32 = 3;
const MAX_WORLD_UNITS: usize = 64;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnitView {
    pub x: f32,
    pub z: f32,
    pub heading: f32,
    pub team: u32,
    pub alive: u32,
    pub kind: u32,
    pub support_pulse: f32,
}

#[derive(Clone, Debug)]
struct Unit {
    x: f32,
    z: f32,
    heading: f32,
    team: u32,
    kind: u32,
    health: f32,
    max_health: f32,
    attack_timer: f32,
    support_timer: f32,
    support_pulse: f32,
    alive: bool,
}

impl Unit {
    fn new(x: f32, z: f32, team: u32, kind: u32, seed: usize) -> Self {
        let (max_health, attack_timer) = match kind {
            MEDIC => (82.0, 0.0),
            CATAPULT => (155.0, 0.0),
            MODERN => (125.0, 0.0),
            _ => (105.0, 0.2 + (seed % 4) as f32 * 0.08),
        };
        Self {
            x, z, heading: if team == 0 { 1.5708 } else { -1.5708 },
            team, kind, health: max_health, max_health,
            attack_timer, support_timer: 0.15 + (seed % 5) as f32 * 0.08,
            support_pulse: 0.0, alive: true,
        }
    }

    fn view(&self) -> UnitView {
        UnitView {
            x: self.x, z: self.z, heading: self.heading,
            team: self.team, alive: u32::from(self.alive),
            kind: self.kind, support_pulse: self.support_pulse,
        }
    }
}

#[derive(Clone, Debug)]
struct World {
    units: Vec<Unit>,
    winner: i32,
    reset_timer: f32,
}

impl World {
    fn new() -> Self {
        // A mixed formation: line units, medics, engineers and unarmed supply trucks.
        const KINDS: [u32; 20] = [
            SWORDSMAN, MEDIC, SWORDSMAN, CATAPULT, MODERN,
            SWORDSMAN, SWORDSMAN, MEDIC, SWORDSMAN, MODERN,
            SWORDSMAN, CATAPULT, SWORDSMAN, MEDIC, SWORDSMAN,
            MODERN, SWORDSMAN, CATAPULT, SWORDSMAN, SWORDSMAN,
        ];
        let mut units = Vec::with_capacity(TOTAL_UNITS);
        for team in 0..2u32 {
            for row in 0..4 {
                for col in 0..5 {
                    let index = row * 5 + col;
                    let lane = -3.0 + row as f32 * 2.0;
                    let offset = col as f32 * 0.42;
                    let x = if team == 0 { -11.5 + offset } else { 11.5 - offset };
                    let z = lane + (col % 2) as f32 * 0.18;
                    units.push(Unit::new(x, z, team, KINDS[index], index + team as usize));
                }
            }
        }
        Self { units, winner: 0, reset_timer: 0.0 }
    }

    fn alive_count(&self, team: u32) -> usize {
        self.units.iter().filter(|u| u.alive && u.team == team).count()
    }

    fn combat_count(&self, team: u32) -> usize {
        self.units.iter().filter(|u| u.alive && u.team == team && u.kind != MEDIC).count()
    }

    fn spawn(&mut self, team: u32, kind: u32) -> bool {
        if team > 1 || kind > MODERN || self.units.len() >= MAX_WORLD_UNITS { return false; }
        let ordinal = self.units.iter().filter(|u| u.team == team).count();
        let row = ordinal / 5;
        let col = ordinal % 5;
        let x = if team == 0 { -12.0 + row as f32 * 0.45 } else { 12.0 - row as f32 * 0.45 };
        let z = -4.0 + col as f32 * 1.6;
        self.units.push(Unit::new(x, z, team, kind, ordinal));
        true
    }

    fn update(&mut self, dt: f32) {
        if self.winner != 0 {
            self.reset_timer -= dt;
            if self.reset_timer <= 0.0 { *self = Self::new(); }
            return;
        }

        for i in 0..self.units.len() {
            if !self.units[i].alive { continue; }
            self.units[i].support_pulse = (self.units[i].support_pulse - dt).max(0.0);
            let team = self.units[i].team;
            let kind = self.units[i].kind;
            let x = self.units[i].x;
            let z = self.units[i].z;

            // Support roles never attack: they move toward damaged allies and restore them.
            if kind == MEDIC {
                let mut target: Option<(usize, f32, f32, f32)> = None;
                for (j, ally) in self.units.iter().enumerate() {
                    if j == i || !ally.alive || ally.team != team || ally.health >= ally.max_health - 0.5 { continue; }
                    let dx = ally.x - x;
                    let dz = ally.z - z;
                    let dist_sq = dx * dx + dz * dz;
                    if target.map(|t| dist_sq < t.1).unwrap_or(true) { target = Some((j, dist_sq, dx, dz)); }
                }
                if let Some((target_index, dist_sq, dx, dz)) = target {
                    let distance = dist_sq.sqrt();
                    if distance > 0.0001 { self.units[i].heading = dx.atan2(dz); }
                    if distance > 1.15 {
                        let step = (1.55 * dt).min(distance - 1.15);
                        if distance > 0.0001 {
                            self.units[i].x += dx / distance * step;
                            self.units[i].z += dz / distance * step;
                        }
                    } else {
                        self.units[target_index].health =
                            (self.units[target_index].health + 18.0 * dt).min(self.units[target_index].max_health);
                        self.units[i].support_pulse = 0.18;
                        self.units[target_index].support_pulse = 0.18;
                    }
                }
                continue;
            }

            // Sphere classes use a simplified abstract game engagement.
            let mut target: Option<(usize, f32, f32, f32)> = None;
            for (j, opponent) in self.units.iter().enumerate() {
                if !opponent.alive || opponent.team == team { continue; }
                let dx = opponent.x - x;
                let dz = opponent.z - z;
                let dist_sq = dx * dx + dz * dz;
                if target.map(|t| dist_sq < t.1).unwrap_or(true) { target = Some((j, dist_sq, dx, dz)); }
            }
            let Some((target_index, dist_sq, dx, dz)) = target else { continue; };
            let distance = dist_sq.sqrt();
            if distance > 0.0001 { self.units[i].heading = dx.atan2(dz); }

            if distance > 0.76 {
                let step = (1.65 * dt).min(distance - 0.76);
                if distance > 0.0001 {
                    self.units[i].x += dx / distance * step;
                    self.units[i].z += dz / distance * step;
                }
            } else {
                self.units[i].attack_timer -= dt;
                if self.units[i].attack_timer <= 0.0 {
                    self.units[i].attack_timer = 0.62;
                    self.units[target_index].health -= 25.0;
                    if self.units[target_index].health <= 0.0 { self.units[target_index].alive = false; }
                }
            }
        }

        let blue = self.combat_count(0);
        let red = self.combat_count(1);
        if blue == 0 || red == 0 {
            self.winner = if blue == 0 && red == 0 { 3 } else if blue == 0 { 2 } else { 1 };
            self.reset_timer = RESET_DELAY;
        }
    }
}

static WORLD: Mutex<Option<World>> = Mutex::new(None);

fn with_world<R>(f: impl FnOnce(&mut World) -> R) -> Option<R> {
    let mut guard = WORLD.lock().ok()?;
    if guard.is_none() { *guard = Some(World::new()); }
    guard.as_mut().map(f)
}

#[no_mangle]
pub extern "C" fn conflict_reset() { let _ = with_world(|world| *world = World::new()); }

#[no_mangle]
pub extern "C" fn conflict_update(delta_seconds: f32) {
    if !delta_seconds.is_finite() || delta_seconds <= 0.0 { return; }
    let _ = with_world(|world| world.update(delta_seconds.min(0.05)));
}

#[no_mangle]
pub extern "C" fn conflict_get_units(output: *mut UnitView, capacity: usize) -> usize {
    with_world(|world| {
        let count = world.units.iter().filter(|unit| unit.alive).count();
        if output.is_null() || capacity == 0 { return count; }
        let mut written = 0;
        for unit in &world.units {
            if !unit.alive || written >= capacity { continue; }
            // The caller supplies a writable output array with the stated capacity.
            unsafe { output.add(written).write(unit.view()); }
            written += 1;
        }
        written
    }).unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn conflict_round_state() -> i32 { with_world(|world| world.winner).unwrap_or(0) }

#[no_mangle]
pub extern "C" fn conflict_spawn(team: u32, kind: u32) -> i32 {
    with_world(|world| i32::from(world.spawn(team, kind))).unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn conflict_alive_count(team: u32) -> u32 {
    with_world(|world| world.alive_count(team) as u32).unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn starts_with_mixed_units_per_team() {
        let world = World::new();
        assert_eq!(world.units.len(), TOTAL_UNITS);
        assert_eq!(world.alive_count(0), 20);
        assert_eq!(world.alive_count(1), 20);
        assert_eq!(world.units.iter().filter(|u| u.team == 0 && u.kind == MEDIC).count(), 3);
        assert_eq!(world.units.iter().filter(|u| u.team == 0 && u.kind == CATAPULT).count(), 3);
        assert_eq!(world.units.iter().filter(|u| u.team == 0 && u.kind == MODERN).count(), 3);
    }

    #[test]
    fn line_units_advance_toward_the_opposing_side() {
        let mut world = World::new();
        let index = world.units.iter().position(|u| u.kind == SWORDSMAN && u.team == 0).unwrap();
        let before = world.units[index].x;
        world.update(0.05);
        assert!(world.units[index].x > before);
    }

    #[test]
    fn support_units_heal_damaged_allies() {
        let mut world = World::new();
        let medic = world.units.iter().position(|u| u.kind == MEDIC && u.team == 0).unwrap();
        let patient = world.units.iter().position(|u| u.kind == SWORDSMAN && u.team == 0).unwrap();
        world.units[medic].x = world.units[patient].x;
        world.units[medic].z = world.units[patient].z;
        world.units[patient].health -= 40.0;
        let before = world.units[patient].health;
        world.update(0.05);
        assert!(world.units[patient].health > before);
    }

    #[test]
    fn winning_team_resets_the_round() {
        let mut world = World::new();
        for unit in &mut world.units {
            if unit.team == 1 && unit.kind != MEDIC { unit.alive = false; }
        }
        world.update(0.01);
        assert_eq!(world.winner, 1);
        world.update(RESET_DELAY + 0.1);
        assert_eq!(world.winner, 0);
        assert_eq!(world.alive_count(0), 20);
        assert_eq!(world.alive_count(1), 20);
    }

    #[test]
    fn spawn_adds_selected_sphere_class_to_selected_team() {
        let mut world = World::new();
        assert!(world.spawn(0, CATAPULT));
        assert_eq!(world.units.last().unwrap().team, 0);
        assert_eq!(world.units.last().unwrap().kind, CATAPULT);
        assert!(!world.spawn(3, MODERN));
    }

    #[test]
    fn long_running_mixed_simulation_keeps_finite_state() {
        let mut world = World::new();
        for _ in 0..20_000 {
            world.update(0.025);
            for unit in &world.units {
                assert!(unit.x.is_finite() && unit.z.is_finite());
                assert!(unit.health.is_finite() && unit.support_pulse.is_finite());
            }
        }
    }
}