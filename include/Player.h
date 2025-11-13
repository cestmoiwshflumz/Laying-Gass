#ifndef PLAYER_H
#define PLAYER_H

#include <string>

/**
 * @brief Représente un joueur humain, son score et ses ressources bonus.
 */
class Player {
public:
    /**
     * @brief Crée un joueur nommé avec le symbole qui identifie ses tuiles.
     */
    Player(const std::string& name, char symbol);

    /** @name Informations générales */
    ///@{
    const std::string& getName() const;
    char getSymbol() const;
    int getScore() const;
    void incrementScore(int points);
    ///@}

    /** @name Coupons d'échange */
    ///@{
    int getSwapCoupons() const;
    bool hasSwapCoupon() const;
    void addSwapCoupon();
    bool useSwapCoupon();
    ///@}

    /** @name Pierres bloquantes */
    ///@{
    int getStones() const;
    bool hasStone() const;
    void addStone();
    bool useStone();
    ///@}

    /** @name Jetons de vol */
    ///@{
    int getStealTokens() const;
    bool hasStealToken() const;
    void addStealToken();
    bool useStealToken();
    ///@}

private:
    std::string name;
    char symbol;
    int score;
    int swapCoupons;
    int stones;
    int stealTokens;
};

#endif // PLAYER_H
