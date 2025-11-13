#include "Player.h"

Player::Player(const std::string& name, char symbol)
        : name(name), symbol(symbol), score(0), swapCoupons(1), stones(0), stealTokens(0) {}

const std::string& Player::getName() const {
    return name;
}

char Player::getSymbol() const {
    return symbol;
}

int Player::getScore() const {
    return score;
}

void Player::incrementScore(int points) {
    score += points;
}

int Player::getSwapCoupons() const {
    return swapCoupons;
}

bool Player::hasSwapCoupon() const {
    return swapCoupons > 0;
}

void Player::addSwapCoupon() {
    ++swapCoupons;
}

bool Player::useSwapCoupon() {
    if (!hasSwapCoupon()) {
        return false;
    }
    --swapCoupons;
    return true;
}

int Player::getStones() const {
    return stones;
}

bool Player::hasStone() const {
    return stones > 0;
}

void Player::addStone() {
    ++stones;
}

bool Player::useStone() {
    if (!hasStone()) {
        return false;
    }
    --stones;
    return true;
}

int Player::getStealTokens() const {
    return stealTokens;
}

bool Player::hasStealToken() const {
    return stealTokens > 0;
}

void Player::addStealToken() {
    ++stealTokens;
}

bool Player::useStealToken() {
    if (!hasStealToken()) {
        return false;
    }
    --stealTokens;
    return true;
}
