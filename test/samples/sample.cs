// a small class
using System;

namespace App
{
    public class Greeter
    {
        private readonly string name;

        public Greeter(string name)
        {
            this.name = name;
        }

        public string Greet(int times)
        {
            return $"Hello {name}" + new string('!', times);
        }
    }
}
