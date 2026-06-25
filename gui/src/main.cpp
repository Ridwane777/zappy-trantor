#include "app/GuiApp.hpp"
#include "GUIException.hpp"

#include <exception>
#include <iostream>

int main(int argc, char **argv)
{
    try {
        return GuiApp{}.run(argc, argv);
    } catch (const std::exception &e) {
