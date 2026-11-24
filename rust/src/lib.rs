//! Real-time, abstract sphere-unit simulation for Conflict.
use std::sync::Mutex;

const UNITS_PER_TEAM: usize = 20;
const TOTAL_UNITS: usize = UNITS_PER_TEAM * 2;
const MOVE_SPEED: f32 = 1.65;
const ENGAGE_DISTANCE: f32 = 0.72;
const ATTACK_INTERVAL: f32 = 0.62;
const ATTACK_POWER: f32 = 25.0;
const RESET_DELAY: f32 = 2.6;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnitView {
    pub x: f32,
    pub z: f32,
    pub team: u32,
    pub alive: u32,
}

#[derive(Clone, Debug)]
struct Unit {
    x: f32,
    z: f32,
    team: u32,
    health: f32,
    attack_timer: f32,
    alive: bool,
}
impl Unit {
    fn view(&self) -> UnitView {
        UnitView { x: self.x, z: self.z, team: self.team, alive: u32::from(self.alive) }
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
        let mut units = Vec::with_capacity(TOTAL_UNITS);
        for team in 0..2u32 {
            for row in 0..4 {
                for col in 0..5 {
                    let lane = -3.0 + row as f32 * 2.0;
                    let offset = col as f32 * 0.42;
                    let x = if team == 0 { -11.5 + offset } else { 11.5 - offset };
                    units.push(Unit {
                        x,
                        z: lane + (col % 2) as f32 * 0.18,
                        team,
                        health: 100.0,
                        attack_timer: 0.2 + ((row + col) % 4) as f32 * 0.08,
                        alive: true,
                    });
                }
            }
        }
        Self { units, winner: 0, reset_timer: 0.0 }
    }

    fn alive_count(&self, team: u32) -> usize {
        self.units.iter().filter(|u| u.alive && u.team == team).count()
    }

    fn update(&mut self, dt: f32) {
        if self.winner != 0 {
            self.reset_timer -= dt;
            if self.reset_timer <= 0.0 {
                *self = Self::new();
            }
            return;
        }

        // Each living sphere selects its closest opponent and advances in real time.
        for i in 0..self.units.len() {
            if !self.units[i].alive { continue; }
            let team = self.units[i].team;
            let x = self.units[i].x;
            let z = self.units[i].z;

            let mut target: Option<(usize, f32, f32, f32)> = None;
            for (j, opponent) in self.units.iter().enumerate() {
                if !opponent.alive || opponent.team == team { continue; }
                let dx = opponent.x - x;
                let dz = opponent.z - z;
                let dist_sq = dx * dx + dz * dz;
                if target.map(|t| dist_sq < t.1).unwrap_or(true) {
                    target = Some((j, dist_sq, dx, dz));
                }
            }
            let Some((target_index, dist_sq, dx, dz)) = target else { continue; };
            let distance = dist_sq.sqrt();

            if distance > ENGAGE_DISTANCE {
                let step = (MOVE_SPEED * dt).min(distance - ENGAGE_DISTANCE);
                if distance > 0.0001 {
                    self.units[i].x += (dx / distance) * step;
                    self.units[i].z += (dz / distance) * step;
                }
            } else {
                self.units[i].attack_timer -= dt;
                if self.units[i].attack_timer <= 0.0 {
                    self.units[i].attack_timer = ATTACK_INTERVAL;
                    self.units[target_index].health -= ATTACK_POWER;
                    if self.units[target_index].health <= 0.0 {
                        self.units[target_index].alive = false;
                    }
                }
            }
        }

        let blue = self.alive_count(0);
        let red = self.alive_count(1);
        if blue == 0 {
            self.winner = 2;
            self.reset_timer = RESET_DELAY;
        } else if red == 0 {
            self.winner = 1;
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
pub extern "C" fn conflict_reset() {
    let _ = with_world(|world| *world = World::new());
}

#[no_mangle]
pub extern "C" fn conflict_update(delta_seconds: f32) {
    if !delta_seconds.is_finite() || delta_seconds <= 0.0 { return; }
    let dt = delta_seconds.min(0.05);
    let _ = with_world(|world| world.update(dt));
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
pub extern "C" fn conflict_round_state() -> i32 {
    with_world(|world| world.winner).unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn conflict_alive_count(team: u32) -> u32 {
    with_world(|world| world.alive_count(team) as u32).unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn starts_with_twenty_units_per_team() {
        let world = World::new();
        assert_eq!(world.units.len(), TOTAL_UNITS);
        assert_eq!(world.alive_count(0), 20);
        assert_eq!(world.alive_count(1), 20);
        assert_eq!(world.winner, 0);
    }

    #[test]
    fn units_advance_toward_the_opposing_side() {
        let mut world = World::new();
        let before = world.units[0].x;
        world.update(0.05);
        assert!(world.units[0].x > before);
        let red_before = world.units[UNITS_PER_TEAM].x;
        world.update(0.05);
        assert!(world.units[UNITS_PER_TEAM].x < red_before);
    }

    #[test]
    fn winning_team_resets_the_round() {
        let mut world = World::new();
        for unit in &mut world.units {
            if unit.team == 1 { unit.alive = false; }
        }
        world.update(0.01);
        assert_eq!(world.winner, 1);
        world.update(RESET_DELAY + 0.1);
        assert_eq!(world.winner, 0);
        assert_eq!(world.alive_count(0), 20);
        assert_eq!(world.alive_count(1), 20);
    }

    #[test]
    fn a_battle_eventually_has_a_winner() {
        let mut world = World::new();
        for _ in 0..12_000 {
            world.update(0.025);
            if world.winner != 0 { break; }
        }
        assert!(world.winner == 1 || world.winner == 2);
    }
}