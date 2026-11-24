//! Abstract troop counters and fictional territory state for Conflict.
use std::sync::Mutex;

const N:usize=6;
const LIMIT:i32=9_999;
const ADJ:[[bool;N];N]=[
 [false,true,true,false,false,false],
 [true,false,true,false,true,false],
 [true,true,false,true,true,false],
 [false,false,true,false,true,true],
 [false,true,true,true,false,true],
 [false,false,false,true,true,false],
];
#[derive(Clone,Debug)]
struct World{troops:[i32;N],owners:[i32;N],day:i32}
impl World{
 const fn new()->Self{Self{troops:[245,180,320,155,210,95],owners:[0,1,0,1,2,2],day:1}}
 fn total(&self)->i32{self.troops.iter().sum()}
 fn recruit(&mut self,region:usize,amount:i32)->bool{
  if region>=N||amount<=0||amount>500{return false;}
  let Some(v)=self.troops[region].checked_add(amount)else{return false;};
  if v>LIMIT{return false;}self.troops[region]=v;true
 }
 fn next_turn(&mut self)->i32{
  self.day=self.day.saturating_add(1);
  for n in &mut self.troops{*n=n.saturating_add(5).min(LIMIT);}self.day
 }
 fn transfer(&mut self,from:usize,to:usize,amount:i32)->bool{
  if from>=N||to>=N||from==to||amount<=0||!ADJ[from][to]||self.troops[from]<amount{return false;}
  if self.troops[to].saturating_add(amount)>LIMIT{return false;}
  self.troops[from]-=amount;self.troops[to]+=amount;true
 }
}
static WORLD:Mutex<World>=Mutex::new(World::new());
#[no_mangle]pub extern "C" fn conflict_reset(){if let Ok(mut w)=WORLD.lock(){*w=World::new();}}
#[no_mangle]pub extern "C" fn conflict_region_count()->i32{N as i32}
#[no_mangle]pub extern "C" fn conflict_region_troops(i:i32)->i32{let Ok(w)=WORLD.lock()else{return 0;};let Ok(i)=usize::try_from(i)else{return 0;};w.troops.get(i).copied().unwrap_or(0)}
#[no_mangle]pub extern "C" fn conflict_region_owner(i:i32)->i32{let Ok(w)=WORLD.lock()else{return 0;};let Ok(i)=usize::try_from(i)else{return 0;};w.owners.get(i).copied().unwrap_or(0)}
#[no_mangle]pub extern "C" fn conflict_total_troops(_unused:i32)->i32{WORLD.lock().map(|w|w.total()).unwrap_or(0)}
#[no_mangle]pub extern "C" fn conflict_current_day(_unused:i32)->i32{WORLD.lock().map(|w|w.day).unwrap_or(1)}
#[no_mangle]pub extern "C" fn conflict_recruit(i:i32,amount:i32)->i32{let Ok(i)=usize::try_from(i)else{return 0;};WORLD.lock().map(|mut w|i32::from(w.recruit(i,amount))).unwrap_or(0)}
#[no_mangle]pub extern "C" fn conflict_next_turn()->i32{WORLD.lock().map(|mut w|w.next_turn()).unwrap_or(0)}
#[no_mangle]pub extern "C" fn conflict_transfer(a:i32,b:i32,n:i32)->i32{let(Ok(a),Ok(b))=(usize::try_from(a),usize::try_from(b))else{return 0;};WORLD.lock().map(|mut w|i32::from(w.transfer(a,b,n))).unwrap_or(0)}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn initial_world_has_six_regions_and_1205_units(){let w=World::new();assert_eq!(N,6);assert_eq!(w.total(),1205);assert_eq!(w.day,1);}
 #[test]fn recruit_changes_selected_region(){let mut w=World::new();assert!(w.recruit(2,50));assert_eq!(w.troops[2],370);assert!(!w.recruit(2,-1));assert!(!w.recruit(20,10));}
 #[test]fn turn_adds_reinforcements(){let mut w=World::new();assert_eq!(w.next_turn(),2);assert_eq!(w.troops[0],250);}
 #[test]fn transfers_require_adjacent_regions(){let mut w=World::new();assert!(w.transfer(0,2,25));assert_eq!(w.troops[0],220);assert_eq!(w.troops[2],345);assert!(!w.transfer(0,5,10));assert!(!w.transfer(0,2,10000));}
}