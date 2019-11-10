#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <vector>
#include "board.h"
#include "action.h"
#include "weight.h"
static const int TUPLE_NUM = 4;
static const int TUPLE_LENGTH = 6;
static const int way = 8;
static const int boardsize = 16;
static const long long MAX_INDEX = 15 * 15 * 15 * 15 * 15 * 15;

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

class weight_agent : public agent {
public:
	weight_agent(const std::string& args = "") : agent(args) {
		if (meta.find("init") != meta.end()) // pass init=... to initialize the weight
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end()) // pass load=... to load from a specific file
			load_weights(meta["load"]);
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end()) // pass save=... to save to a specific file
			save_weights(meta["save"]);
	}

protected:
	virtual void init_weights(const std::string& info) {
		net.emplace_back(65536); // create an empty weight table with size 65536
		net.emplace_back(65536); // create an empty weight table with size 65536
		// now net.size() == 2; net[0].size() == 65536; net[1].size() == 65536
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}

protected:
	std::vector<weight> net;
};

/**
 * base agent for agents with a learning rate
 */
class learning_agent : public agent {
public:
	learning_agent(const std::string& args = "") : agent(args), alpha(0.1f) {
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
	}
	virtual ~learning_agent() {}

protected:
	float alpha;
};
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
			if(after(pos) != 0) continue;
			
			int tile = bg.get_tile();
			return action::place(pos, tile);
		}
		return action();

	}

private:
	std::array<int, 16> space;
	std::uniform_int_distribution<int> popup;
};

/**
 * dummy player
 * select a legal action randomly
 */
class player : public weight_agent {
public:
	player(const std::string& args = "") : weight_agent("name=dummy role=player " + args),
		opcode({ 0, 1, 2, 3 }) {
			for (int i=0; i<TUPLE_NUM; i++)
				net.push_back(weight(MAX_INDEX));
		}

	virtual action take_action(const board& before, tile_bag &bg) {
		//std::shuffle(opcode.begin(), opcode.end(), engine);
		// std::cout << "I am player " << '\n';
		
		int best_op = -1;
		float v_s = -999999;
		board after;
		int best_reward = 0;

		for (int op : opcode) {

			board next = before;
			board::reward reward = next.slide(op);

			if (reward != -1) {
				float v_s_tmp = load_value(next) + reward;
				if(v_s_tmp > v_s) {
					best_op = op;
					v_s = v_s_tmp;
					after = next;
					best_reward = reward;
				}
			}
		}
		if(best_op == -1)
			return action();
		else{
			episodes.push_back(state{after, best_reward});
			return action::slide(best_op);
		}
	
	}
	virtual void open_episode(const std::string& flag = "" ) {
		episodes.clear();
		episodes.reserve(357600);
	} 

private:
	std::array<int, 4> opcode;
	struct state {
		board after;
		int reward;
	};
	std::vector<state> episodes;
private:
	long get_tuple_value(const board &b,const std::array<int, boardsize> arr) {
		long result = 0;
		int c = 15;
		std::vector<int> pos(6, -1);
		for (int i=0; i < boardsize; i++) {
			if (arr[i] == 0) continue;
			pos[ arr[i] - 1 ] = *(&(b[0][0]) + i);
			// result *= c;
			// result += *(&(b[0][0]) + i);
		}

		for(auto &a: pos ) {
			
			if(a == -1) continue;
			result *= c;
			// int tile = *(&(b[0][0]) + i);
			result += a;
			
		}
		return result;
	}

	void reflect_horizontal(std::array<int, 16> &arr) {
		
		for (int i = 0; i < 4 ; i++) {
			std::swap(arr[ (i<<2)    ], arr[ (i<<2) + 3]);
			std::swap(arr[ (i<<2) + 1], arr[ (i<<2) + 2]);
		}
	}
	void transpose(std::array<int, 16> &arr) {
		for (int i = 0; i < 4; i++) 
			for (int j = i + 1; j < 4 ; j++) {
				std::swap(arr[ (i<<2) + j], arr[ (j<<2) + i]);
			}
	}
	void rotate(std::array<int, 16> &arr) {
		transpose(arr); reflect_horizontal(arr);
	}	
	double load_value(const board &b) {
		double total = 0;		
		std::array<std::array<int, boardsize > ,TUPLE_NUM > arr = n_tuple;
		
		for (int i = 0; i < TUPLE_NUM; i++) {
			for (int j = 0; j < 2 ; j++) {
				reflect_horizontal(arr[i]);
				for (int k = 0; k < 4; k++) {
					total += net[i][ get_tuple_value(b, arr[i] )];
					rotate(arr[i]);
				}
			}
		}
		return total;
	}
	void print(std::array<int, 16> &arr) {
		for(int i=0; i<4; i++){
			for(int j=0; j<4; j++)
				std::cout << arr[ (i<<2)+j] << " ";
			std::cout << '\n';
		}
		std::cout << '\n';
	}
	void train_weight(board &now , board &next , int reward, int firstin) {
		std::array<std::array<int, boardsize> ,TUPLE_NUM> arr = n_tuple;
		double rate = 0.003125f;
		double data;
		if(firstin) data = rate * (-load_value(now));
		else data = rate * (reward + load_value(next) - load_value(now));

		
		for (int i = 0; i < TUPLE_NUM; i++) {
			for (int j = 0; j < 2 ; j++) {
				reflect_horizontal(arr[i]);
				for (int k = 0; k < 4; k++) {
					net[i][get_tuple_value(now, arr[i] )] += data;
					rotate(arr[i]);
				}
			}
		}

	}
private:
	std::array<std::array<int, boardsize> ,TUPLE_NUM> n_tuple = {{
		{{1, 6, 0, 0,
		 2, 5, 0, 0,
		 3, 4, 0, 0,
		 0, 0, 0, 0}},

		{{0, 1, 6, 0,
		 0, 2, 5, 0,
		 0, 3, 4, 0,
		 0, 0, 0, 0}},
		
		{{0, 0, 1, 0,
		 0, 0, 2, 0,
		 0, 0, 3, 0,
		 0, 0, 4, 0}},

		{{0, 0, 0, 1,
		 0, 0, 0, 2,
		 0, 0, 0, 3,
		 0, 0, 0, 4}}
	}};
public:
	virtual void close_episode(const std::string& flag = "") {
		
		unsigned int siz = episodes.size();
		train_weight(episodes[siz - 1].after, episodes[siz - 1].after, 0, 1);
		for (int i = siz - 2 ; i >= 0; i--) {
			state next = episodes[i+1];
			train_weight(episodes[i].after , next.after , next.reward, 0);
		}

	}

};
