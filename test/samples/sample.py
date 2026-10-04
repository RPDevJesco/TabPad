# word counts
import sys
from collections import Counter


class Report:
    def __init__(self, path: str):
        self.path = path
        self.total = None

    def run(self, limit=10):
        with open(self.path) as f:
            words = Counter(f.read().split())
        for word, n in words.most_common(limit):
            print(f"{word}: {n}")
        return len(words) > 0


if __name__ == "__main__":
    Report(sys.argv[1]).run()
