#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b, tile_bag & bg) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};
//modified
// class tile_bag : public random_agent {
// 	//bag contain 1, 2, 3
// 	public:
// 		tile_bag() : siz(0), bag({1, 2, 3}) {}
// 		int get_tile(){
// 			if (siz == 0) {
// 				//refilled bag
// 				std::shuffle(bag.begin(), bag.end(), engine);
// 				siz = 3;
// 			}
// 			siz--;
// 			return bag[siz];
// 		}
// 	private:
// 		std::array<int, 3> bag;
// 		int siz;
// };

/**
 * random environment
 * add a new random tile to an empty cell
 * 2-tile: 90%
 * 4-tile: 10%
 */
class rndenv : public random_agent {
public:
	rndenv(const std::string& args = "") : random_agent("name=random role=environment " + args),
		space({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }), popup(0, 9) {}

	virtual action take_action(const board& after, tile_bag &bg) {
		// std::cout << "I am rndenv " << '\n';
		std::array<int, 4> arr = {0, 1, 2, 3};
		std::array<int, 16> arr_init = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
		int dir = after.get_dir();
		
		// std::cout << "dir: " << dir << '\n';
		
		std::shuffle(arr_init.begin(), arr_init.end(), engine);

		if(dir == -2) {
			for (auto pos : arr_init) {
				if(after(pos) != 0) continue;
				int tile = bg.get_tile();
				return action::place(pos, tile);
			}
		}
		switch (dir) {
			case 0: //slide up by player
				arr = {12, 13, 14, 15};
				break;
			case 1: //slide right by player
				arr = {0, 4, 8, 12};
				break;
			case 2: //slide down by player
				arr = {0, 1, 2, 3};
				break;
			case 3: //slide left by player
				arr = {3, 7, 11, 15};
				break;				
		}
		std::shuffle(arr.begin(), arr.end(), engine);
		// tile_bag bag;
		for (auto pos : arr) {
			// std::cout << "?" << '\n';
			if(after(pos) != 0) continue;
			// board tmp = after;
			
			int tile = bg.get_tile();
			return action::place(pos, tile);
		}
		return action();


		// std::shuffle(space.begin(), space.end(), engine);
		// for (int pos : space) {
		// 	if (after(pos) != 0) continue;
		// 	board::cell tile = popup(engine) ? 1 : 2;
		// 	return action::place(pos, tile);
		// }
		// return action();
		
	}

private:
	std::array<int, 16> space;
	std::uniform_int_distribution<int> popup;
};

/**
 * dummy player
 * select a legal action randomly
 */
class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=dummy role=player " + args),
		opcode({ 0, 1, 2, 3 }) {}

	virtual action take_action(const board& before, tile_bag &bg) {
		std::shuffle(opcode.begin(), opcode.end(), engine);
		// std::cout << "I am player " << '\n';
		
		int maxx = -3;
		int maxop;
		for (int op : opcode) {
			board::reward reward = board(before).slide(op);
			if (reward > maxx) {
				maxx = reward;
				maxop = op;
			}
			// if (reward != -1) return action::slide(op);
		}
		return action::slide(maxop);
		// return action();
	}

private:
	std::array<int, 4> opcode;
};
