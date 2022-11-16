#include "vidulum/JoinSplit.hpp"

#include <iostream>
#include "crypto/common.h"

int main(int argc, char **argv)
{
    if (init_and_check_sodium() == -1) {
        return 1;
    }

    if(argc != 4) {
        std::cerr << "Usage: " 