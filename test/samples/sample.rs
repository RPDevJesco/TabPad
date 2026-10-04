// a tally
use std::collections::HashMap;

#[derive(Debug)]
struct Tally {
    counts: HashMap<String, u32>,
}

impl Tally {
    fn add(&mut self, word: &str) -> u32 {
        let n = self.counts.entry(word.to_string()).or_insert(0);
        *n += 1;
        *n
    }
}

fn main() {
    let mut t = Tally { counts: HashMap::new() };
    println!("{}", t.add("one"));
}
