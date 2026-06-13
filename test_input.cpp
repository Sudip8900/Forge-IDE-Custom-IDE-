#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main() {
    std::cout << "C++ program started.\n";
    std::cout << "Enter your favorite programming language: ";
    std::string lang;
    if (std::getline(std::cin, lang)) {
        std::cout << "Awesome, you like " << lang << "!\n";
    }
    for (int i = 0; i < 3; ++i) {
        std::cout << "Tick " << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "Done!\n";
    return 0;
}
