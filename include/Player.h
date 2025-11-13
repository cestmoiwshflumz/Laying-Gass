#ifndef PLAYER_H
#define PLAYER_H

#include <string>

class Player {
public:
    Player(const std::string& name, char symbol);

    const std::string& getName() const;
    char getSymbol() const;
    int getScore() const;
    void incrementScore(int points);

    int getSwapCoupons() const;
    bool hasSwapCoupon() const;
    void addSwapCoupon();
    bool useSwapCoupon();

    int getStones() const;
    bool hasStone() const;
    void addStone();
    bool useStone();

    int getStealTokens() const;
    bool hasStealToken() const;
    void addStealToken();
    bool useStealToken();

private:
    std::string name;
    char symbol;
    int score;
    int swapCoupons;
    int stones;
    int stealTokens;
};

#endif // PLAYER_H
