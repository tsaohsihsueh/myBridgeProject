#include <iostream>
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <map>
#include <iomanip>
#include <array>
#include <sstream>
#include <fstream>
#include <stdexcept>

using namespace std;

#include "dll.h"

int outputcount = 0;

string  vulner[8] = { "0", "N", "E", "B", "N", "E", "B", "0" };
int dealernum[8] = { 3, 4, 1, 2, 3, 4, 1, 2 };

// A helper function to split a string by a delimiter.
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief Converts a single hand from PBN's dot-separated format to LIN's suit-prefixed format.
 */
std::string convertHandToLinFormat(const std::string& pbnHand) {
    std::vector<std::string> suits = split(pbnHand, '.');
    if (suits.size() != 4) {
        return "InvalidHand";
    }

    std::stringstream linHand;
    linHand << "S" << suits[0] << "H" << suits[1] << "D" << suits[2] << "C" << suits[3];
    return linHand.str();
}

/**
 * @brief Converts a full PBN deal string to a BBO-compatible LIN format string.
 */
std::string pbnToLin(const std::string& pbnString, int index) {
    size_t colonPos = pbnString.find(':');
    if (colonPos == std::string::npos || colonPos == 0) {
        throw std::invalid_argument("Invalid PBN format: Missing or misplaced dealer info.");
    }

    std::string allHandsStr = pbnString.substr(colonPos + 1);

    std::vector<std::string> pbnHands = split(allHandsStr, ' ');
    pbnHands.erase(std::remove_if(pbnHands.begin(), pbnHands.end(), [](const std::string& s) {
        return s.empty();
        }), pbnHands.end());

    if (pbnHands.size() != 4) {
        throw std::invalid_argument("Invalid PBN format: Must contain exactly four hands.");
    }

    std::map<char, std::string> playerHands;
    const std::string players = "SNWE";

    for (int i = 0; i < 4; ++i) {
        char currentPlayer = players[i % 4];
        playerHands[currentPlayer] = pbnHands[i];
    }

    std::stringstream linString;
    linString << "qx|o" << to_string(index) << "|md|" << dealernum[index - 1];

    linString << convertHandToLinFormat(playerHands['S']) << ",";
    linString << convertHandToLinFormat(playerHands['W']) << ",";
    linString << convertHandToLinFormat(playerHands['N']) << ",";
    linString << convertHandToLinFormat(playerHands['E']);

    linString << "|rh||ah|Board " << index << "|sv|" << vulner[index - 1] << "|pg||";

    return linString.str();
}

bool pbnToDeal(const string& pbn, deal& dl) {
    int player = 0, suit = 3, r;
    char c;
    for (unsigned int i = 0; i < pbn.size(); i++) {
        if (pbn[i] == '.')suit--;
        if (pbn[i] == ' ') { player++; suit = 3; }
        c = pbn[i];
        switch (toupper(c)) {
        case 'A': r = 14; break;
        case 'K': r = 13; break;
        case 'Q': r = 12; break;
        case 'J': r = 11; break;
        case 'T': r = 10; break;
        default:
            if (isdigit(c)) r = c - '0';
            else continue;
        }
        if (r >= 2 && r <= 14) {
            dl.remainCards[player][suit] |= (1 << (r));
        }
    }
    return true;
}

void printDeal(const deal& dl) {
    const char* players[] = { "North", "East", "South", "West" };
    const char* suits[] = { "C", "D", "H", "S" };
    const char* rankStr = "23456789TJQKA";

    cout << '\n';
    for (int player = 0; player < 4; ++player) {
        if ((player == 1) || (player == 3))continue;
        cout << players[player] << ":\n";
        for (int suit = 3; suit >= 0; --suit) {
            cout << "  " << suits[suit] << ": ";
            unsigned cards = dl.remainCards[player][suit];
            for (int r = 14; r >= 2; --r) {
                if (cards & (1 << (r))) {
                    cout << rankStr[r - 2];
                }
            }
            cout << endl;
        }
    }
}

// FIX: Takes ofstream by reference to avoid opening the file multiple times
void printDealforOutput(const deal& dl, ofstream& out) {
    const char* players[] = { "North", "East", "South", "West" };
    const char* suits[] = { "C", "D", "H", "S" };
    const char* rankStr = "23456789TJQKA";

    out << '\n';
    for (int player = 0; player < 4; ++player) {
        if ((player == 1) || (player == 3))continue;
        out << players[player] << ":\n";
        for (int suit = 3; suit >= 0; --suit) {
            out << "  " << suits[suit] << ": ";
            unsigned cards = dl.remainCards[player][suit];
            for (int r = 14; r >= 2; --r) {
                if (cards & (1 << (r))) {
                    out << rankStr[r - 2];
                }
            }
            out << endl;
        }
    }
}

string convertPBN(vector<int>& v) {
    const char ranks[] = { 'A', 'K', 'Q', 'J', 'T', '9', '8', '7', '6', '5', '4', '3', '2' };

    std::array<std::array<std::vector<char>, 4>, 4> hands;

    for (int i = 0; i < 52; i++) {
        int card = v[i];
        int suit = card / 13;
        int rank = card % 13;
        int player = i / 13;
        char rank_char = ranks[12 - rank];
        hands[player][suit].push_back(rank_char);
    }

    std::string pbn = "N:";

    for (int player = 0; player < 4; player++) {
        if (player > 0) pbn += " ";

        for (int suit = 0; suit < 4; suit++) {
            if (suit > 0) pbn += ".";

            if (!hands[player][suit].empty()) {
                std::sort(hands[player][suit].begin(), hands[player][suit].end(),
                    [](char a, char b) {
                        const char order[] = "AKQJT98765432";
                        return std::string(order).find(a) < std::string(order).find(b);
                    });

                for (char card : hands[player][suit]) {
                    pbn += card;
                }
            }
        }
    }

    return pbn;
}

// FIX: Takes ofstream by reference; writes HCP inline without reopening the file
void hcpforOutput(vector<int>& v, ofstream& out) {
    int p = 0;
    int c[13] = { 0,0,0,0,0,0,0,0,0,1,2,3,4 };
    for (int i = 13; i < 26; i++) {
        p += c[v[i] % 13];
    }
    out << p << " ";
    p = 0;
    for (int i = 0; i < 13; i++) {
        p += c[v[i] % 13];
    }
    out << p;
}

void hcpforConsole(vector<int>& v) {
    int p = 0;
    int c[13] = { 0,0,0,0,0,0,0,0,0,1,2,3,4 };
    for (int i = 13; i < 26; i++) {
        p += c[v[i] % 13];
    }
    cout << p << " ";
    p = 0;
    for (int i = 0; i < 13; i++) {
        p += c[v[i] % 13];
    }
    cout << p;
}

uint32_t get_high_res_time_seed() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint32_t>(counter.QuadPart);
}

boardsPBN bo;
solvedBoards solved;
int res;

void solveAll(vector<int>& v) {
    mt19937 g(get_high_res_time_seed());
    vector<int>h, p;
    h.resize(52); p.resize(52);
    copy(v.begin(), v.end(), h.begin());
    copy(h.begin(), h.begin() + 13, p.begin() + 26);
    copy(h.begin() + 13, h.begin() + 26, p.begin());

    copy(h.begin() + 26, h.begin() + 39, p.begin() + 39);
    copy(h.begin() + 39, h.begin() + 52, p.begin() + 13);
    deal dl;
    memset(&dl, 0, sizeof(dl));
    pbnToDeal(convertPBN(p), dl);
    cout << '\n' << "hcp(N S):";
    hcpforConsole(h);
    printDeal(dl);

    int num = 100;
    bo.noOfBoards = num;

    cout << '\n' << "  ";
    for (int i = 7; i < 14; i++)cout << setw(4) << i;
    cout << '\n';
    for (int i = 0; i < 5; i++) {
        for (int handno = 0; handno < num; handno++)
        {
            bo.deals[handno].trump = i;
            bo.deals[handno].first = 3;

            bo.deals[handno].currentTrickSuit[0] = 0;
            bo.deals[handno].currentTrickSuit[1] = 0;
            bo.deals[handno].currentTrickSuit[2] = 0;

            bo.deals[handno].currentTrickRank[0] = 0;
            bo.deals[handno].currentTrickRank[1] = 0;
            bo.deals[handno].currentTrickRank[2] = 0;

            copy(h.begin() + 26, h.begin() + 39, p.begin() + 39);
            copy(h.begin() + 39, h.begin() + 52, p.begin() + 13);
            string pbn = convertPBN(p);
            shuffle(h.begin() + 26, h.end(), g);

            strcpy_s(bo.deals[handno].remainCards, sizeof(bo.deals[handno].remainCards), pbn.c_str());

            bo.target[handno] = -1;
            bo.solutions[handno] = 3;
            bo.mode[handno] = 0;
        }

        res = SolveAllBoards(&bo, &solved);

        if (res != RETURN_NO_FAULT)
        {
            printf("DDS error: %d\n", res);
        }

        char s[5] = { 'S','H','D','C','N' };
        int c[14];
        for (int i = 0; i < 14; i++)c[i] = 0;
        int sum = 0;
        for (int j = 0; j < num; j++) {
            sum += solved.solvedBoard[j].score[0];
            c[solved.solvedBoard[j].score[0]]++;
        }
        for (int i = 1; i < 14; i++)c[i] += c[i - 1];
        cout << s[i] << ":";
        for (int i = 6; i >= 0; i--)cout << setw(4) << c[i];
        cout << setw(6) << fixed << setprecision(2) << 13 - (double)sum / (double)num << '\n';
    }
}

// FIX: Opens the file once per hand (trunc for hand 1, app for hands 2-8),
//      passes the stream to helpers instead of letting them reopen it.
//      Also restored the "Hand N" label.
void solveAllforOutput(vector<int>& v, int handcnt) {
    mt19937 g(get_high_res_time_seed());
    vector<int>h, p;
    h.resize(52); p.resize(52);
    copy(v.begin(), v.end(), h.begin());
    copy(h.begin(), h.begin() + 13, p.begin() + 26);
    copy(h.begin() + 13, h.begin() + 26, p.begin());

    copy(h.begin() + 26, h.begin() + 39, p.begin() + 39);
    copy(h.begin() + 39, h.begin() + 52, p.begin() + 13);
    deal dl;
    memset(&dl, 0, sizeof(dl));
    pbnToDeal(convertPBN(p), dl);

    // FIX: Open file once here; truncate on first hand, append on subsequent hands
    ofstream out;
    string outputFilename = "evaluation" + to_string(outputcount) + ".txt";
    auto mode = (handcnt == 1) ? (std::ios_base::out | std::ios_base::trunc)
        : (std::ios_base::out | std::ios_base::app);
    out.open(outputFilename, mode);

    if (out.fail()) {
        cout << "Failed to open evaluation file.\n";
        return;
    }

    // FIX: Restored hand label so output is clearly separated
    out << "\n--- Hand " << handcnt << " ---\n";
    out << "hcp(N S): ";
    // FIX: Pass stream to helper instead of reopening inside it
    hcpforOutput(h, out);
    printDealforOutput(dl, out);

    int num = 100;
    bo.noOfBoards = num;

    out << '\n' << "  ";
    for (int i = 7; i < 14; i++) out << setw(4) << i;
    out << '\n';

    for (int i = 0; i < 5; i++) {
        for (int handno = 0; handno < num; handno++)
        {
            bo.deals[handno].trump = i;
            bo.deals[handno].first = 3;

            bo.deals[handno].currentTrickSuit[0] = 0;
            bo.deals[handno].currentTrickSuit[1] = 0;
            bo.deals[handno].currentTrickSuit[2] = 0;

            bo.deals[handno].currentTrickRank[0] = 0;
            bo.deals[handno].currentTrickRank[1] = 0;
            bo.deals[handno].currentTrickRank[2] = 0;

            copy(h.begin() + 26, h.begin() + 39, p.begin() + 39);
            copy(h.begin() + 39, h.begin() + 52, p.begin() + 13);
            string pbn = convertPBN(p);
            shuffle(h.begin() + 26, h.end(), g);

            strcpy_s(bo.deals[handno].remainCards, sizeof(bo.deals[handno].remainCards), pbn.c_str());

            bo.target[handno] = -1;
            bo.solutions[handno] = 3;
            bo.mode[handno] = 0;
        }

        res = SolveAllBoards(&bo, &solved);

        if (res != RETURN_NO_FAULT)
        {
            printf("DDS error: %d\n", res);
        }

        char s[5] = { 'S','H','D','C','N' };
        int c[14];
        for (int i = 0; i < 14; i++) c[i] = 0;
        int sum = 0;
        for (int j = 0; j < num; j++) {
            sum += solved.solvedBoard[j].score[0];
            c[solved.solvedBoard[j].score[0]]++;
        }
        for (int i = 1; i < 14; i++) c[i] += c[i - 1];
        out << s[i] << ":";
        for (int i = 6; i >= 0; i--) out << setw(4) << c[i];
        out << setw(6) << fixed << setprecision(2) << 13 - (double)sum / (double)num << '\n';
    }
    // out closes automatically here (RAII)
}

void printpar(string pbnDeal) {
    ddTableDealPBN tableDealPBN;
    strcpy_s(tableDealPBN.cards, sizeof(tableDealPBN.cards), pbnDeal.c_str());

    ddTableResults tableResults;
    CalcDDtablePBN(tableDealPBN, &tableResults);

    parResultsDealer dealerParRes;
    int dealer = 2;
    int vulnerable = 0;

    DealerPar(&tableResults, &dealerParRes, dealer, vulnerable);

    std::cout << "Par Score: " << dealerParRes.score << std::endl;
    std::cout << "Par Contracts: ";
    for (int i = 0; i < dealerParRes.number; i++) {
        if (i > 0) std::cout << ", ";
        std::cout << dealerParRes.contracts[i];
    }
    std::cout << "\n";
}

struct Card {
    std::string suit;
    char symbol;
    std::string rank;
    int value;
    int hcp;

    Card(const std::string& s, char sym, const std::string& r, int v, int h)
        : suit(s), symbol(sym), rank(r), value(v), hcp(h) {
    }
};

struct SuitLengths {
    int spade = 0;
    int heart = 0;
    int diamond = 0;
    int club = 0;
};

struct HandProperties {
    int hcp = 0;
    SuitLengths suitLengths;
};

class BridgeDealer {
private:
    std::vector<Card> deck;
    std::mt19937 rng;

    const std::vector<std::string> suits = { "spade", "heart", "diamond", "club" };
    const std::vector<char> suitSymbols = { 'S', 'H', 'D', 'C' };
    const std::vector<std::string> ranks = { "A", "K", "Q", "J", "10", "9", "8", "7", "6", "5", "4", "3", "2" };
    const std::map<std::string, int> rankValues = {
        {"A", 14}, {"K", 13}, {"Q", 12}, {"J", 11}, {"10", 10}, {"9", 9},
        {"8", 8}, {"7", 7}, {"6", 6}, {"5", 5}, {"4", 4}, {"3", 3}, {"2", 2}
    };
    const std::map<std::string, int> hcpValues = {
        {"A", 4}, {"K", 3}, {"Q", 2}, {"J", 1}
    };

public:
    BridgeDealer() : rng(get_high_res_time_seed()) {}

    void createAndShuffleDeck() {
        deck.clear();
        for (size_t i = 0; i < suits.size(); ++i) {
            for (const auto& rank : ranks) {
                int hcp = (hcpValues.find(rank) != hcpValues.end()) ? hcpValues.at(rank) : 0;
                deck.emplace_back(suits[i], suitSymbols[i], rank, rankValues.at(rank), hcp);
            }
        }
        std::shuffle(deck.begin(), deck.end(), rng);
    }

    HandProperties getHandProperties(const std::vector<Card>& hand) {
        HandProperties props;
        props.hcp = 0;

        for (const auto& card : hand) {
            props.hcp += card.hcp;
            if (card.suit == "spade") props.suitLengths.spade++;
            else if (card.suit == "heart") props.suitLengths.heart++;
            else if (card.suit == "diamond") props.suitLengths.diamond++;
            else if (card.suit == "club") props.suitLengths.club++;
        }

        return props;
    }

    bool isBalanced(const SuitLengths& lengths) {
        std::vector<int> lens = { lengths.spade, lengths.heart, lengths.diamond, lengths.club };

        for (int len : lens) {
            if (len < 2) return false;
        }

        int doubletons = 0;
        for (int len : lens) {
            if (len == 2) doubletons++;
        }

        return doubletons <= 1;
    }

    bool isValidForOpening(const std::vector<Card>& hand, const std::string& opening) {
        HandProperties props = getHandProperties(hand);
        int hcp = props.hcp;
        SuitLengths& s = props.suitLengths;
        bool balanced = isBalanced(s);

        if (opening == "1NT") {
            return hcp >= 15 && hcp <= 17 && balanced && s.spade < 5 && s.heart < 5;
        }
        else if (opening == "1S") {
            return hcp >= 12 && hcp <= 21 && s.spade >= 5;
        }
        else if (opening == "1H") {
            return hcp >= 12 && hcp <= 21 && s.heart >= 5 && s.heart > s.spade;
        }
        else if (opening == "1D") {
            return hcp >= 12 && hcp <= 21 && s.diamond >= 3 && s.heart < 5 && s.spade < 5 &&
                s.diamond >= s.club &&
                !(hcp >= 15 && hcp <= 17 && balanced && s.spade < 5 && s.heart < 5) &&
                !(hcp >= 20 && hcp <= 21 && balanced && s.spade < 5 && s.heart < 5) &&
                !(s.diamond == 3 && s.club == 3);
        }
        else if (opening == "1C") {
            return hcp >= 12 && hcp <= 21 && s.club >= 3 && s.heart < 5 && s.spade < 5 &&
                s.club >= s.diamond &&
                !(hcp >= 15 && hcp <= 17 && balanced && s.spade < 5 && s.heart < 5) &&
                !(hcp >= 20 && hcp <= 21 && balanced && s.spade < 5 && s.heart < 5) &&
                !(s.diamond == 4 && s.club == 4);
        }
        else if (opening == "2C") {
            return hcp >= 22 &&
                !(hcp >= 25 && hcp <= 27 && balanced);
        }
        else if (opening == "2D") {
            return hcp >= 5 && hcp <= 11 && s.diamond >= 6;
        }
        else if (opening == "2H") {
            return hcp >= 5 && hcp <= 11 && s.heart >= 6;
        }
        else if (opening == "2S") {
            return hcp >= 5 && hcp <= 11 && s.spade >= 6;
        }
        else if (opening == "2NT") {
            return hcp >= 20 && hcp <= 21 && balanced && s.spade < 5 && s.heart < 5;
        }
        else if (opening == "3C") {
            return hcp >= 6 && hcp <= 10 && s.club >= 7;
        }
        else if (opening == "3D") {
            return hcp >= 6 && hcp <= 10 && s.diamond >= 7;
        }
        else if (opening == "3H") {
            return hcp >= 6 && hcp <= 10 && s.heart >= 7;
        }
        else if (opening == "3S") {
            return hcp >= 6 && hcp <= 10 && s.spade >= 7;
        }
        else if (opening == "3NT") {
            return hcp >= 25 && hcp <= 27 && balanced;
        }

        return true;
    }

    std::map<std::string, std::vector<Card>> dealCards(const std::string& selectedOpening) {
        std::vector<Card> southHand;
        std::vector<Card> otherCards;

        if (selectedOpening == "any") {
            createAndShuffleDeck();
            southHand.assign(deck.begin(), deck.begin() + 13);
            otherCards.assign(deck.begin() + 13, deck.end());
        }
        else {
            int attempts = 0;
            while (attempts < 100000) {
                createAndShuffleDeck();
                std::vector<Card> candidateHand(deck.begin(), deck.begin() + 13);
                if (isValidForOpening(candidateHand, selectedOpening)) {
                    southHand = candidateHand;
                    otherCards.assign(deck.begin() + 13, deck.end());
                    break;
                }
                attempts++;
            }
            if (southHand.empty()) {
                std::cout << "Could not generate a suitable hand in a reasonable number of attempts.\n";
                return {};
            }
        }

        std::map<std::string, std::vector<Card>> hands;
        hands["south"] = southHand;
        hands["north"] = std::vector<Card>(otherCards.begin(), otherCards.begin() + 13);
        hands["west"] = std::vector<Card>(otherCards.begin() + 13, otherCards.begin() + 26);
        hands["east"] = std::vector<Card>(otherCards.begin() + 26, otherCards.end());

        return hands;
    }

    std::vector<int> dealintCards(const std::string& selectedOpening) {
        std::vector<Card> southHand;
        std::vector<Card> otherCards;

        if (selectedOpening == "any") {
            createAndShuffleDeck();
        }
        else {
            int attempts = 0;
            bool b = false;
            while (attempts < 100000) {
                createAndShuffleDeck();
                std::vector<Card> candidateHand(deck.begin(), deck.begin() + 13);
                if (isValidForOpening(candidateHand, selectedOpening)) {
                    b = true;
                    break;
                }
                attempts++;
            }
            if (!b) {
                std::cout << "Could not generate a suitable hand in a reasonable number of attempts.\n";
                return {};
            }
        }

        std::vector<int> inthands(52);
        for (int i = 0; i < 52; i++) {
            auto ptr1 = find(suits.begin(), suits.end(), deck[i].suit);
            auto ptr2 = find(ranks.begin(), ranks.end(), deck[i].rank);
            int id1 = ptr1 - suits.begin();
            int id2 = ptr2 - ranks.begin();
            id2 = 12 - id2;
            inthands[i] = id1 * 13 + id2;
        }

        return inthands;
    }

    bool isparpointsValid(int hcp, int parpoints) {
        if (parpoints == 1) {
            if (hcp >= 0 && hcp < 6)return true;
            else return false;
        }
        else if (parpoints == 2) {
            if (hcp >= 6 && hcp < 13)return true;
            else return false;
        }
        else if (parpoints == 3) {
            if (hcp >= 13)return true;
            else return false;
        }
        return true;
    }

    std::vector<int> dealintCardsandHCP(const std::string& selectedOpening, int parpoints) {

        if (selectedOpening == "any") {
            createAndShuffleDeck();
        }
        else {
            int attempts = 0;
            bool b1 = false, b2 = false;
            while (attempts < 100000) {
                createAndShuffleDeck();
                std::vector<Card> candidateHand(deck.begin(), deck.begin() + 13);
                if (isValidForOpening(candidateHand, selectedOpening)) {
                    b1 = true;
                    for (int i = 0; i < 10000; i++) {
                        std::vector<Card> northHand(deck.begin() + 13, deck.begin() + 26);
                        HandProperties northprops = getHandProperties(northHand);
                        if (isparpointsValid(northprops.hcp, parpoints)) {
                            b2 = true;
                            break;
                        }
                        shuffle(deck.begin() + 13, deck.end(), rng);
                    }
                    break;
                }
                attempts++;
            }
            if (!b1 || !b2) {
                std::cout << "Could not generate a suitable hand in a reasonable number of attempts.\n";
                return {};
            }
        }

        std::vector<int> inthands(52);
        for (int i = 0; i < 52; i++) {
            auto ptr1 = find(suits.begin(), suits.end(), deck[i].suit);
            auto ptr2 = find(ranks.begin(), ranks.end(), deck[i].rank);
            int id1 = ptr1 - suits.begin();
            int id2 = ptr2 - ranks.begin();
            id2 = 12 - id2;
            inthands[i] = id1 * 13 + id2;
        }

        return inthands;
    }

    std::string convertToPBN(const std::map<std::string, std::vector<Card>>& hands) {
        const std::vector<std::string> suitOrder = { "spade", "heart", "diamond", "club" };
        const std::vector<std::string> playerOrder = { "north", "east", "south", "west" };

        std::string pbn = "N:";

        for (size_t playerIdx = 0; playerIdx < playerOrder.size(); ++playerIdx) {
            if (playerIdx > 0) pbn += " ";

            const std::string& player = playerOrder[playerIdx];
            auto hand = hands.at(player);

            std::sort(hand.begin(), hand.end(), [&suitOrder](const Card& a, const Card& b) {
                auto suitIndexA = std::find(suitOrder.begin(), suitOrder.end(), a.suit) - suitOrder.begin();
                auto suitIndexB = std::find(suitOrder.begin(), suitOrder.end(), b.suit) - suitOrder.begin();
                if (suitIndexA != suitIndexB) return suitIndexA < suitIndexB;
                return a.value > b.value;
                });

            std::map<std::string, std::vector<Card>> groupedBySuit;
            for (const auto& card : hand) {
                groupedBySuit[card.suit].push_back(card);
            }

            for (size_t suitIdx = 0; suitIdx < suitOrder.size(); ++suitIdx) {
                if (suitIdx > 0) pbn += ".";

                const std::string& suitName = suitOrder[suitIdx];
                if (groupedBySuit.find(suitName) != groupedBySuit.end()) {
                    for (const auto& card : groupedBySuit[suitName]) {
                        std::string rank = card.rank;
                        if (rank == "10") rank = "T";
                        pbn += rank;
                    }
                }
            }
        }

        return pbn;
    }

    void displayHands(const std::map<std::string, std::vector<Card>>& hands) {
        const std::vector<std::string> suitOrder = { "spade", "heart", "diamond", "club" };
        const std::vector<std::string> playerOrder = { "north", "east", "south", "west" };

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "                    BRIDGE HAND DEALER\n";
        std::cout << std::string(60, '=') << "\n\n";

        for (const auto& player : playerOrder) {
            std::cout << std::setw(10) << std::left << (player + ":") << "\n";

            auto hand = hands.at(player);
            std::sort(hand.begin(), hand.end(), [&suitOrder](const Card& a, const Card& b) {
                auto suitIndexA = std::find(suitOrder.begin(), suitOrder.end(), a.suit) - suitOrder.begin();
                auto suitIndexB = std::find(suitOrder.begin(), suitOrder.end(), b.suit) - suitOrder.begin();
                if (suitIndexA != suitIndexB) return suitIndexA < suitIndexB;
                return a.value > b.value;
                });

            std::map<std::string, std::vector<Card>> groupedBySuit;
            for (const auto& card : hand) {
                groupedBySuit[card.suit].push_back(card);
            }

            for (const auto& suitName : suitOrder) {
                if (groupedBySuit.find(suitName) != groupedBySuit.end()) {
                    char symbol = suitSymbols[std::find(suits.begin(), suits.end(), suitName) - suits.begin()];
                    std::cout << "  " << symbol << " ";
                    for (size_t i = 0; i < groupedBySuit[suitName].size(); ++i) {
                        if (i > 0) std::cout << " ";
                        std::cout << groupedBySuit[suitName][i].rank;
                    }
                    std::cout << "\n";
                }
            }

            if (player == "south") {
                HandProperties props = getHandProperties(hand);
                std::cout << "  HCP: " << props.hcp << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "PBN: " << convertToPBN(hands) << "\n";
    }

    void showOpeningMenu() {
        std::cout << "\nAvailable Opening Bids:\n";
        std::cout << "0.  Any (random)\n";
        std::cout << "1.  1C    2.  1D    3.  1H    4.  1S    5.  1NT\n";
        std::cout << "6.  2C    7.  2D    8.  2H    9.  2S    10. 2NT\n";
        std::cout << "11. 3C    12. 3D    13. 3H    14. 3S    15. 3NT\n";
        std::cout << "\nSelect opening bid (0-15): ";
    }

    std::string getOpeningFromChoice(int choice) {
        const std::vector<std::string> openings = {
            "any", "1C", "1D", "1H", "1S", "1NT",
            "2C", "2D", "2H", "2S", "2NT",
            "3C", "3D", "3H", "3S", "3NT"
        };

        if (choice >= 0 && choice < static_cast<int>(openings.size())) {
            return openings[choice];
        }
        return "any";
    }

    void showPartnersHCP() {
        cout << "\nSelect partner's HCP range\n";
        cout << "0: Any\n";
        cout << "1: 0-5\n";
        cout << "2: 6-12\n";
        cout << "3: 13+\n";
    }

    void run() {
        std::cout << "Force South's opening bid or deal completely random hands.\n";

        deal dl = {};
        futureTricks fut = {};
        memset(&dl, 0, sizeof(deal));
        dl.first = 3;
        showOpeningMenu();

        int choice;
        std::cin >> choice;

        if (choice == -1) {
            std::cout << "Goodbye!\n";
            return;
        }

        std::string opening = getOpeningFromChoice(choice);
        std::cout << "\nDealing cards";
        if (opening != "any") {
            std::cout << " (forcing South to open " << opening << ")";
        }
        std::cout << "...\n";

        auto hands = dealCards(opening);
        if (!hands.empty()) {
            std::string pbn = convertToPBN(hands);
            pbnToDeal(pbn, dl);
            displayHands(hands);
            printpar(pbn);
        }
    }

    void run2() {
        std::cout << "Force South's opening bid or deal completely random hands.\n";

        deal dl = {};
        memset(&dl, 0, sizeof(deal));
        dl.first = 3;
        showOpeningMenu();

        int choice;
        std::cin >> choice;

        showPartnersHCP();
        int parpoints;
        cin >> parpoints;

        if (choice == -1) {
            std::cout << "Goodbye!\n";
            return;
        }

        std::string opening = getOpeningFromChoice(choice);
        std::cout << "\nDealing cards";
        if (opening != "any") {
            std::cout << " (forcing South to open " << opening << ")";
        }
        std::cout << "...\n";

        vector<int> inthands = dealintCardsandHCP(opening, parpoints);
        solveAll(inthands);
    }

    void run3() {
        std::cout << "Force South's opening bid or deal completely random hands.\n";

        deal dl = {};
        futureTricks fut = {};
        memset(&dl, 0, sizeof(deal));
        dl.first = 3;
        showOpeningMenu();

        int choice;
        std::cin >> choice;

        showPartnersHCP();
        int parpoints;
        cin >> parpoints;

        if (choice == -1) {
            std::cout << "Goodbye!\n";
            return;
        }

        std::string opening = getOpeningFromChoice(choice);
        std::cout << "\nDealing cards";
        if (opening != "any") {
            std::cout << " (forcing South to open " << opening << ")";
        }
        std::cout << "...\n";

        // FIX: Increment outputcount BEFORE generating any files,
        //      so both the .lin and evaluation .txt share the same index
        ++outputcount;

        ofstream out;
        string linFilename = "output" + to_string(outputcount) + ".lin";
        out.open(linFilename, std::ios_base::trunc); // FIX: trunc so each run3() starts fresh

        if (out.fail()) {
            cout << "Failed to open output file.\n";
            return; // FIX: return instead of exit so the program can continue
        }

        cout << "Generating 8 hands...\n";
        for (int i = 1; i <= 8; i++) {
            vector<int> inthands = dealintCardsandHCP(opening, parpoints);
            string pbn = convertPBN(inthands);
            // FIX: solveAllforOutput now uses outputcount (already incremented above)
            //      and handles trunc/app internally based on handcnt
            solveAllforOutput(inthands, i);
            std::string linDeal = pbnToLin(pbn, i);
            out << linDeal << "\n";
        }
        // out closes here automatically
    }
};

int main() {
    BridgeDealer dealer;

    std::cout << "Welcome to Bridge Hand Dealer!\n";

    while (true) {
        cout << "option 1: par\n";
        cout << "option 2: 1 hand\n";
        cout << "option 3: 8 hands\n";
        int option;
        cin >> option;
        switch (option) {
        case 1:
            dealer.run();
            break;
        case 2:
            dealer.run2();
            break;
        case 3:
            dealer.run3();
            break;
        }

        std::cout << "\nPress Enter to restart (or enter -1 to quit): ";
        std::cin.ignore();
        std::string input;
        std::getline(std::cin, input);
        if (input == "-1") {
            std::cout << "Goodbye!\n";
            break;
        }
    }

    return 0;
}
