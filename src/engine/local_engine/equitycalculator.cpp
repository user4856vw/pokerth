/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/

#include "equitycalculator.h"

#include "handevaluator.h"
#include "holecardsrange.h"
#include "tools.h"

#include <random>
#include <utility>
#include <cmath>
#include <algorithm>

namespace
{

// Fast generator for the simulation. The entropy comes from Tools::GetRand()
// once per thread; using its boost::random_device directly would be far too
// slow for a hundred thousand random numbers per decision.
std::mt19937& simulationRng()
{
	static thread_local bool seeded = false;
	static thread_local std::mt19937 rng;
	if(!seeded) {
		int seed[2];
		Tools::GetRand(0, 0x7FFFFFFF, 2, seed);
		std::seed_seq sequence{ seed[0], seed[1] };
		rng.seed(sequence);
		seeded = true;
	}
	return rng;
}

inline unsigned nextBelow(std::mt19937& rng, unsigned bound)
{
	return static_cast<unsigned>(rng()) % bound;
}

typedef std::pair<signed char, signed char> CardPair;

// All hole card combinations that match a range and do not use an already
// known card. Built up in advance so that tight ranges do not get expensive
// through repeated rejection.
void buildCombos(const HoleCardsRange* range, unsigned long long knownMask, std::vector<CardPair>& out)
{
	out.clear();
	out.reserve(1326);
	for(int a = 0; a < 51; ++a) {
		if(knownMask & (1ULL << a)) continue;
		for(int b = a + 1; b < 52; ++b) {
			if(knownMask & (1ULL << b)) continue;
			if(range && !range->contains(a, b)) continue;
			out.push_back(CardPair(static_cast<signed char>(a), static_cast<signed char>(b)));
		}
	}
}

}

EquityCalculator::Result EquityCalculator::equity(const int* holeCards,
		const int* boardCards, int boardSize,
		const std::vector<const HoleCardsRange*>& opponentRanges,
		int samples, std::uint32_t seed, bool jointRejection)
{
	Result result = { 0.0, 0.0, 0.0, 0, 0.0 };

	const int opponents = static_cast<int>(opponentRanges.size());
	if(opponents < 1 || samples < 1 || boardSize < 0 || boardSize > 5) return result;

	HandEvaluator::init();

	// Block the known cards.
	unsigned long long knownMask = 0;
	int known[7];
	int knownCount = 0;
	known[knownCount++] = holeCards[0];
	known[knownCount++] = holeCards[1];
	for(int i = 0; i < boardSize; ++i) known[knownCount++] = boardCards[i];
	for(int i = 0; i < knownCount; ++i) {
		if(known[i] < 0 || known[i] > 51) return result;
		if(knownMask & (1ULL << known[i])) return result;  // doppelte Karte
		knownMask |= 1ULL << known[i];
	}

	// Zulaessige Kombinationen je Gegner.
	std::vector< std::vector<CardPair> > combos(opponents);
	for(int o = 0; o < opponents; ++o) {
		buildCombos(opponentRanges[o], knownMask, combos[o]);
		// A range that is empty after subtracting the known cards cannot be
		// simulated -- in that case arbitrary hands are better than no result at all.
		if(combos[o].empty() && !jointRejection) buildCombos(0, knownMask, combos[o]);
		if(combos[o].empty()) return result;
	}

	// A nonzero seed is an isolated, replayable stream for local training.
	// Unseeded engine callers keep their existing thread-local generator.
	std::mt19937 seededRng(seed);
	std::mt19937& rng = seed != 0 ? seededRng : simulationRng();

	const int missingBoard = 5 - boardSize;
	int myCards[7];
	myCards[0] = holeCards[0];
	myCards[1] = holeCards[1];

	std::vector<int> opponentCards(2 * opponents);
	double equitySum = 0.0;
	double equitySumSquares = 0.0;
	int wins = 0, ties = 0, played = 0;

	// In joint-rejection mode, each independently drawn opponent combo is
	// accepted only if the entire multi-player deal has no collisions.
	// Resampling a later opponent until compatible would overweight earlier
	// hands that block many possibilities from that opponent's narrow range.
	// Bound the work when assigned ranges conflict almost completely.
	const int maxAttempts = jointRejection ? samples * 120 : samples;
	for(int s = 0; s < maxAttempts && (!jointRejection || played < samples); ++s) {
		unsigned long long usedMask = knownMask;
		bool usable = true;

		// Gegnerhaende ziehen.
		for(int o = 0; o < opponents && usable; ++o) {
			const std::vector<CardPair>& list = combos[o];
			bool placed = false;
			// For manual ranges, try exactly once per opponent, then reject
			// the whole sample on a collision (not merely the later player).
			const int maxRejections = jointRejection ? 1 : 200;
			for(int attempt = 0; attempt < maxRejections && !placed; ++attempt) {
				const CardPair& pick = list[nextBelow(rng, static_cast<unsigned>(list.size()))];
				const unsigned long long bits = (1ULL << pick.first) | (1ULL << pick.second);
				if(usedMask & bits) continue;
				usedMask |= bits;
				opponentCards[2 * o] = pick.first;
				opponentCards[2 * o + 1] = pick.second;
				placed = true;
			}
			if(!placed) usable = false;
		}
		if(!usable) continue;

		// Board vervollstaendigen.
		int board[5];
		for(int i = 0; i < boardSize; ++i) board[i] = boardCards[i];
		for(int i = 0; i < missingBoard; ++i) {
			int card;
			do {
				card = static_cast<int>(nextBelow(rng, 52));
			} while(usedMask & (1ULL << card));
			usedMask |= 1ULL << card;
			board[boardSize + i] = card;
		}

		for(int i = 0; i < 5; ++i) myCards[2 + i] = board[i];
		const unsigned myValue = HandEvaluator::value(myCards);

		// Evaluate the opponents and count who is level with us.
		int equalBest = 0;
		bool beaten = false;
		for(int o = 0; o < opponents; ++o) {
			int hand[7];
			hand[0] = opponentCards[2 * o];
			hand[1] = opponentCards[2 * o + 1];
			for(int i = 0; i < 5; ++i) hand[2 + i] = board[i];
			const unsigned value = HandEvaluator::value(hand);
			if(value > myValue) {
				beaten = true;
				break;
			}
			if(value == myValue) ++equalBest;
		}

		++played;
		if(beaten) continue;
		if(equalBest == 0) {
			++wins;
			equitySum += 1.0;
			equitySumSquares += 1.0;
		} else {
			++ties;
			const double share = 1.0 / (equalBest + 1);
			equitySum += share;
			equitySumSquares += share * share;
		}
	}

	if(played > 0) {
		result.equity = equitySum / played;
		result.win = static_cast<double>(wins) / played;
		result.tie = static_cast<double>(ties) / played;
		result.samples = played;
		if (played > 1) {
			const double populationSecondMoment = equitySumSquares / played;
			result.standardError = std::sqrt(std::max(0.0,
				(populationSecondMoment - result.equity * result.equity) / (played - 1)));
		}
	}

	return result;
}

EquityCalculator::Result EquityCalculator::equity(const int* holeCards,
		const int* boardCards, int boardSize,
		int opponents, const HoleCardsRange* range,
		int samples, std::uint32_t seed)
{
	std::vector<const HoleCardsRange*> ranges(opponents > 0 ? opponents : 0, range);
	return equity(holeCards, boardCards, boardSize, ranges, samples, seed, false);
}
