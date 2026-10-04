// fetch and count
import { readFile } from "node:fs/promises";

const LIMIT = 10;

export async function count(path) {
    const text = await readFile(path, "utf8");
    const words = text.split(/\s+/).filter((w) => w.length > 0);

    if (words.length > LIMIT) {
        console.log(`many: ${words.length}`);
    }

    return words.length;
}

class Counter extends Object {
    constructor() {
        super();
        this.total = null;
    }
}
