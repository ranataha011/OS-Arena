#ifndef ROLL_SEED_H
#define ROLL_SEED_H
// Roll number se HP damage formulas ke liye fixed digits.


constexpr unsigned int ROLL_NUMBER = 240500;

constexpr unsigned int ROLL_CORE = 500; 
constexpr unsigned int ROLL_LAST_2 = ROLL_NUMBER % 100;
constexpr unsigned int ROLL_LAST_1 = ROLL_NUMBER % 10;
constexpr unsigned int ROLL_SECOND_LAST = (ROLL_NUMBER / 10) % 10;

#endif 
