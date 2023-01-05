/*
 * libdebug.cpp
 *
 *  Created on: 2012-2-8
 *      Author: Argon
 */

#include "scriptlib.h"
#include "duel.h"
#include "field.h"
#include "card.h"

#define LUA_MODULE Debug
#include "function_array_helper.h"

namespace {

using namespace scriptlib;

LUA_FUNCTION(Message) {
	int top = lua_gettop(L);
	if(top == 0)
		return 0;
	luaL_checkstack(L, 1, nullptr);
	const auto pduel = lua_get<duel*>(L);
	for(int i = 1; i <= top; ++i) {
		const auto* str = luaL_tolstring(L, i, nullptr);
		if(str)
			pduel->handle_message(str, OCG_LOG_TYPE_FROM_SCRIPT);
		lua_pop(L, 1);
	}
	return 0;
}
LUA_FUNCTION(AddCard) {
	check_param_count(L, 6);
	auto code = lua_get<uint32_t>(L, 1);
	auto owner = lua_get<uint8_t>(L, 2);
	auto playerid = lua_get<uint8_t>(L, 3);
	auto location = lua_get<uint16_t>(L, 4);
	auto sequence = lua_get<uint16_t>(L, 5);
	auto position = lua_get<uint8_t>(L, 6);
	bool proc = lua_get<bool, false>(L, 7);
	if(owner != 0 && owner != 1)
		return 0;
	if(playerid != 0 && playerid != 1)
		return 0;
	const auto pduel = lua_get<duel*>(L);
	auto& field = pduel->game_field;
	if(field->is_location_useable(playerid, location, sequence)) {
		card* pcard = pduel->new_card(code);
		pcard->owner = owner;
		if(location == LOCATION_EXTRA && (position == 0 || (pcard->data.type & TYPE_PENDULUM) == 0))
			position = POS_FACEDOWN_DEFENSE;
		pcard->sendto_param.position = position;
		bool pzone = false;
		if(location == LOCATION_PZONE) {
			location = LOCATION_SZONE;
			sequence = field->get_pzone_index(sequence, playerid);
			pzone = true;
		} else if(location == LOCATION_FZONE) {
			location = LOCATION_SZONE;
			sequence = 5;
		} else if(location == LOCATION_STZONE) {
			location = LOCATION_SZONE;
			sequence += 1 * pduel->game_field->is_flag(DUEL_3_COLUMNS_FIELD);

		} else if(location == LOCATION_MMZONE) {
			location = LOCATION_MZONE;
			sequence += 1 * pduel->game_field->is_flag(DUEL_3_COLUMNS_FIELD);

		} else if(location == LOCATION_EMZONE) {
			location = LOCATION_MZONE;
			sequence += 5;
		}
		field->add_card(playerid, pcard, location, sequence, pzone);
		pcard->current.position = position;
		if(!(location & LOCATION_ONFIELD) || (position & POS_FACEUP)) {
			pcard->enable_field_effect(true);
			field->adjust_instant();
		}
		if(proc)
			pcard->set_status(STATUS_PROC_COMPLETE, TRUE);
		interpreter::pushobject(L, pcard);
		return 1;
	} else if(location & LOCATION_ONFIELD) {
		card* pcard = pduel->new_card(code);
		pcard->owner = owner;
		card* fcard = field->get_field_card(playerid, location, sequence);
		fcard->xyz_add(pcard);
		if(proc)
			pcard->set_status(STATUS_PROC_COMPLETE, TRUE);
		interpreter::pushobject(L, pcard);
		return 1;
	}
	return 0;
}
LUA_FUNCTION(SetPlayerInfo) {
	check_param_count(L, 4);
	const auto pduel = lua_get<duel*>(L);
	auto playerid = lua_get<uint8_t>(L, 1);
	auto lp = lua_get<uint32_t>(L, 2);
	auto startcount = lua_get<uint32_t>(L, 3);
	auto drawcount = lua_get<uint32_t>(L, 4);
	if(playerid != 0 && playerid != 1)
		return 0;
	auto& player = pduel->game_field->player[playerid];
	player.lp = lp;
	player.start_lp = lp;
	player.start_count = startcount;
	player.draw_count = drawcount;
	return 0;
}
LUA_FUNCTION(PreSummon) {
	check_param_count(L, 2);
	auto pcard = lua_get<card*, true>(L, 1);
	auto summon_type = lua_get<uint32_t>(L, 2);
	auto summon_location = lua_get<uint8_t, 0>(L, 3);
	auto summon_sequence = lua_get<uint8_t, 0>(L, 4);
	auto summon_pzone = lua_get<bool, false>(L, 5);
	pcard->summon.location = summon_location;
	pcard->summon.type = summon_type;
	pcard->summon.sequence = summon_sequence;
	pcard->summon.pzone = summon_pzone;
	return 0;
}
LUA_FUNCTION(PreEquip) {
	check_param_count(L, 2);
	auto equip_card = lua_get<card*, true>(L, 1);
	auto target = lua_get<card*, true>(L, 2);
	if((equip_card->current.location != LOCATION_SZONE)
	   || (target->current.location != LOCATION_MZONE)
	   || (target->current.position & POS_FACEDOWN))
		lua_pushboolean(L, 0);
	else {
		equip_card->equip(target, FALSE);
		equip_card->effect_target_cards.insert(target);
		target->effect_target_owner.insert(equip_card);
		lua_pushboolean(L, 1);
	}
	return 1;
}
LUA_FUNCTION(PreSetTarget) {
	check_param_count(L, 2);
	auto t_card = lua_get<card*, true>(L, 1);
	auto target = lua_get<card*, true>(L, 2);
	t_card->add_card_target(target);
	return 0;
}
LUA_FUNCTION(PreAddCounter) {
	check_param_count(L, 2);
	auto pcard = lua_get<card*, true>(L, 1);
	auto countertype = lua_get<uint16_t>(L, 2);
	auto count = lua_get<uint16_t>(L, 3);
	uint16_t cttype = countertype & ~COUNTER_NEED_ENABLE;
	auto pr = pcard->counters.emplace(cttype, card::counter_map::mapped_type());
	auto cmit = pr.first;
	if(pr.second) {
		cmit->second[0] = 0;
		cmit->second[1] = 0;
	}
	if((countertype & COUNTER_WITHOUT_PERMIT) && !(countertype & COUNTER_NEED_ENABLE))
		cmit->second[0] += count;
	else
		cmit->second[1] += count;
	return 0;
}
LUA_FUNCTION(ReloadFieldBegin) {
	check_param_count(L, 1);
	const auto pduel = lua_get<duel*>(L);
	auto flag = lua_get<uint64_t>(L, 1);
	auto rule = lua_get<uint8_t, 3>(L, 2);
	bool build = lua_get<bool, false>(L, 3);
	pduel->clear();
#define CHECK(MR) case MR : { flag |= DUEL_MODE_MR##MR; break; }
	if(rule && !build) {
		switch (rule) {
		CHECK(1)
		CHECK(2)
		CHECK(3)
		CHECK(4)
		CHECK(5)
		}
#undef CHECK
	}
	pduel->game_field->core.duel_options = flag;
	return 0;
}
LUA_FUNCTION(ReloadFieldEnd) {
	const auto pduel = lua_get<duel*>(L);
	auto& field = pduel->game_field;
	auto& core = field->core;
	core.shuffle_hand_check[0] = FALSE;
	core.shuffle_hand_check[1] = FALSE;
	core.shuffle_deck_check[0] = FALSE;
	core.shuffle_deck_check[1] = FALSE;
	field->reload_field_info();
	if(lua_isyieldable(L))
		return lua_yield(L, 0);
	return 0;
}
LUA_FUNCTION(GetDuelOptions) {
	const auto pduel = lua_get<duel*>(L);
	lua_pushinteger(L, pduel->game_field->core.duel_options);
	return 1;
}
LUA_FUNCTION(SetDuelOptions) {
	check_param_count(L, 1);
	const auto pduel = lua_get<duel*>(L);
	auto flag = lua_get<uint64_t>(L, 1);
	pduel->game_field->core.duel_options = flag;
	return 0;
}
LUA_FUNCTION(RemoveCardEx) {
	check_param_count(L, 1);
	const auto pduel = lua_get<duel*>(L);
	auto& field = pduel->game_field;
	auto t_card = lua_get<card*, true>(L, 1);
	field->remove_card(t_card);
	return 0;
}
LUA_FUNCTION(AddCardEx) {
	check_param_count(L, 5);
	const auto pduel = lua_get<duel*>(L);
	auto& field = pduel->game_field;
	auto t_card = lua_get<card*, true>(L, 1);
	auto playerid = lua_get<uint8_t>(L, 2);
	auto location = lua_get<uint16_t>(L, 3);
	auto sequence = lua_get<uint16_t>(L, 4);
	auto position = lua_get<uint8_t>(L, 5);
	t_card->current.position = position;
	field->add_card(playerid, t_card, location, sequence);
	return 0;
}
LUA_FUNCTION(NewTsukasaDuel) {
	auto pduel = lua_get<duel*>(L);
	OCG_DuelOptions options;
	options.seed[0] = 0;
	options.seed[1] = 0;
	options.seed[2] = 0;
	options.seed[3] = 0;
	options.cardReader = pduel->read_card_callback;
	options.scriptReader = pduel->read_script_callback;
	options.logHandler = pduel->handle_message_callback;
	options.cardReaderDone = pduel->read_card_done_callback;
	options.payload1 = pduel->read_card_payload;
	options.payload2 = pduel->read_script_payload;
	options.payload3 = pduel->handle_message_payload;
	options.payload4 = pduel->read_card_done_payload;
	auto* qduel = new duel(options);
	qduel->read_script("constant.lua");
	qduel->read_script("utility.lua");
	delete qduel;
	return 0;
}
LUA_FUNCTION(NewTsukasaDuelAlpha) {
	auto pduel = lua_get<duel*>(L);
	if (pduel->playerop_config) {
		return 0;
	}
	OCG_DuelOptions options;
	options.seed[0] = 0;
	options.seed[1] = 0;
	options.seed[2] = 0;
	options.seed[3] = 0;
	char fconf[40] = { 0 };
	sprintf_s(fconf, "./playerop.conf");
	FILE *fpconf = NULL;
	fopen_s(&fpconf, fconf, "r");
	int plconf = 0;
	if (fpconf) {
		char conf[40] = { 0 };
		fgets(conf, 40, fpconf);
		char plop[10] = { 0 };
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(fpconf);
	}
	if (!pduel->playerop_config) {
		char fc[50] = { 0 };
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400] = { 0 };
		fgets(line, 400, fp);
		char curr[25] = { 0 };
		uint64_t seed1 = 0;
		uint64_t seed2 = 0;
		uint64_t seed3 = 0;
		uint64_t seed4 = 0;
		sscanf(line, "%s : %lld,%lld,%lld,%lld", curr, &seed1, &seed2, &seed3, &seed4);
		options.seed[0] = seed1;
		options.seed[1] = seed2;
		options.seed[2] = seed3;
		options.seed[3] = seed4;
		fclose(fp);
	}
	options.cardReader = pduel->read_card_callback;
	options.scriptReader = pduel->read_script_callback;
	options.logHandler = pduel->handle_message_callback;
	options.cardReaderDone = pduel->read_card_done_callback;
	options.payload1 = pduel->read_card_payload;
	options.payload2 = pduel->read_script_payload;
	options.payload3 = pduel->handle_message_payload;
	options.payload4 = pduel->read_card_done_payload;
	options.enableUnsafeLibraries = 1;
	auto* qduel = new duel(options);
	qduel->game_field->core.duel_options = pduel->game_field->core.duel_options;
	qduel->read_script("constant.lua");
	qduel->read_script("utility.lua");
	qduel->playerop_config = 1;
	qduel->playerop_seed[0] = pduel->playerop_seed[0];
	qduel->playerop_seed[1] = pduel->playerop_seed[1];
	qduel->playerop_seed[2] = pduel->playerop_seed[2];
	qduel->playerop_seed[3] = pduel->playerop_seed[3];
	int line_count = 0;
	char kfc[50];
	if (plconf) sprintf_s(kfc, "./playerop.log"); else sprintf_s(kfc, "./playerop %lld.log", pduel->playerop_seed[0]);
	FILE *kfp = NULL;
	fopen_s(&kfp, kfc, "r");
	char line[400];
	while (fgets(line, 400, kfp) != NULL) {
		line_count++;
	}
	fclose(kfp);
	qduel->qlayerop_line = line_count;
	/*char dfc[50];
	if (plconf) sprintf_s(dfc, "./playeropline.log"); else sprintf_s(dfc, "./playeropline %lld.log", options.seed[0]);
	FILE *dfp = NULL;
	fopen_s(&dfp, dfc, "a+");
	fprintf(dfp, "%d", qduel->qlayerop_line);
	fprintf(dfp, "\n");
	fclose(dfp);*/
	if (!pduel->playerop_config) {
		qduel->playerop_line = 0;
		for (int i = 0; i < pduel->playerop_cinfo; i++) {
			char fc[50] = { 0 };
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			OCG_NewCardInfo info = { 0, 0, 0, 0, 0, 0, POS_FACEDOWN_DEFENSE };
			if (!qduel->playerop_line)
				qduel->playerop_line = 2;
			int line_count = 0;
			char line[400] = { 0 };
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == qduel->playerop_line) {
					int currvalue = 0;
					char curr[25] = { 0 };
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "info.team")) {
						info.team = currvalue;
						qduel->playerop_line++;
					}
					else if (!strcmp(curr, "info.duelist")) {
						info.duelist = currvalue;
						qduel->playerop_line++;
					}
					else if (!strcmp(curr, "info.code")) {
						info.code = currvalue;
						qduel->playerop_line++;
					}
					else if (!strcmp(curr, "info.con")) {
						info.con = currvalue;
						qduel->playerop_line++;
					}
					else if (!strcmp(curr, "info.loc")) {
						info.loc = currvalue;
						qduel->playerop_line++;
					}
					else if (!strcmp(curr, "info.seq")) {
						info.seq = currvalue;
						qduel->playerop_line++;
					}
					else if (!strcmp(curr, "info.pos")) {
						info.pos = currvalue;
						qduel->playerop_line++;
						break;
					}
				}
				if (line_count > qduel->playerop_line)
					break;
			}
			if (info.code == 10171017) {
				info.code += info.con;
				info.loc = 0;
				info.seq = 0;
			}
			auto& game_field = *qduel->game_field;
			if (info.duelist == 0) {
				if (game_field.is_location_useable(info.con, info.loc, info.seq)) {
					card* pcard = qduel->new_card(info.code);
					pcard->owner = info.team;
					game_field.add_card(info.con, pcard, (uint8_t)info.loc, (uint8_t)info.seq);
					pcard->current.position = info.pos;
					if (!(info.loc & LOCATION_ONFIELD) || (info.pos & POS_FACEUP)) {
						pcard->enable_field_effect(true);
						game_field.adjust_instant();
					}
					if (info.loc & LOCATION_ONFIELD) {
						if (info.loc == LOCATION_MZONE)
							pcard->set_status(STATUS_PROC_COMPLETE, TRUE);
					}
				}
			}
			else {
				if (!(info.team > 1 || !(info.loc & (LOCATION_DECK | LOCATION_EXTRA)))) {
					card* pcard = qduel->new_card(info.code);
					auto& player = game_field.player[info.team];
					if (info.duelist > player.extra_lists_main.size()) {
						player.extra_lists_main.resize(info.duelist);
						player.extra_lists_extra.resize(info.duelist);
						player.extra_lists_hand.resize(info.duelist);
						player.extra_extra_p_count.resize(info.duelist);
					}
					--info.duelist;
					pcard->current.location = (uint8_t)info.loc;
					pcard->owner = info.team;
					pcard->current.controler = info.team;
					pcard->current.position = POS_FACEDOWN_DEFENSE;
					auto& list = (info.loc == LOCATION_DECK) ? player.extra_lists_main[info.duelist] : player.extra_lists_extra[info.duelist];
					list.push_back(pcard);
					pcard->current.sequence = list.size() - 1;
				}
			}
			fclose(fp);
		}
		qduel->game_field->player[0].start_lp = pduel->game_field->player[0].start_lp;
		qduel->game_field->player[0].lp = qduel->game_field->player[0].start_lp;
		qduel->game_field->player[0].start_count = pduel->game_field->player[0].start_count;
		qduel->game_field->player[0].draw_count = pduel->game_field->player[0].draw_count;
		qduel->game_field->player[1].start_lp = pduel->game_field->player[1].start_lp;
		qduel->game_field->player[1].lp = qduel->game_field->player[1].start_lp;
		qduel->game_field->player[1].start_count = pduel->game_field->player[1].start_count;
		qduel->game_field->player[1].draw_count = pduel->game_field->player[1].draw_count;
		qduel->game_field->add_process(PROCESSOR_STARTUP, 0, 0, 0, 0, 0);
		int stop = 0;
		/*int dodododo = 0;
		int dododododo = 50;
		luaL_checkstack(qduel->lua->lua_state, 1, nullptr);
		lua_getglobal(qduel->lua->lua_state, "playerop_dododododo");
		if (!lua_isnil(qduel->lua->lua_state, -1)) {
			dododododo = lua_tointeger(qduel->lua->lua_state, -1);
		}
		lua_settop(qduel->lua->lua_state, 0);*/
		do {
			qduel->buff.clear();
			int flag = 0;
			//int dododo = 0;
			do {
				/*char dfc[50];
				if (plconf) sprintf_s(dfc, "./playeropdebug.log"); else sprintf_s(dfc, "./playeropdebug %lld.log", options.seed[0]);
				FILE *dfp = NULL;
				fopen_s(&dfp, dfc, "a+");
				fprintf(dfp, "%d,%d", qduel->playerop_line, qduel->qlayerop_line);
				fprintf(dfp, "\n");
				fclose(dfp);*/
				//dododo++;
				flag = qduel->game_field->process();
				qduel->generate_buffer();
			} while (qduel->buff.size() == 0 && flag == PROCESSOR_FLAG_CONTINUE);
			/*if (dododo == 1)
				dodododo++;
			else
				dodododo = 0;
			if (dodododo > dododododo) {
				stop = dododododo;
			}*/
			/*if (qduel->playerop_line >= 0xfffffff)
				stop = 0xfffffff;*/
			stop = (qduel->buff.size() != 0 && flag == PROCESSOR_FLAG_WAITING) || (qduel->playerop_config >= 0xfffffff);
		} while (!stop);
	}
	luaL_checkstack(qduel->lua->lua_state, 2, nullptr);
	lua_getglobal(qduel->lua->lua_state, "playerop_evaluate");
	bool pa = false;
	int count1 = 0;
	int count2 = 0;
	int count3 = 0;
	int count4 = 0;
	int count5 = 0;
	int count6 = 0;
	int count7 = 0;
	if (!lua_isnil(qduel->lua->lua_state, -1)) {
		lua_pcall(qduel->lua->lua_state, 0, 7, 0);
		count1 = lua_tointeger(qduel->lua->lua_state, -7);
		count2 = lua_tointeger(qduel->lua->lua_state, -6);
		count3 = lua_tointeger(qduel->lua->lua_state, -5);
		count4 = lua_tointeger(qduel->lua->lua_state, -4);
		count5 = lua_tointeger(qduel->lua->lua_state, -3);
		count6 = lua_tointeger(qduel->lua->lua_state, -2);
		count7 = lua_tointeger(qduel->lua->lua_state, -1);
		lua_settop(qduel->lua->lua_state, 0);
		pa = true;
	}
	else {
	}
	lua_settop(qduel->lua->lua_state, 0);
	delete qduel;
	if (!pa)
		return 0;
	lua_pushinteger(L, count1);
	lua_pushinteger(L, count2);
	lua_pushinteger(L, count3);
	lua_pushinteger(L, count4);
	lua_pushinteger(L, count5);
	lua_pushinteger(L, count6);
	lua_pushinteger(L, count7);
	return 7;
}
template<int message_code, size_t max_len>
int32_t write_string_message(lua_State* L) {
	check_param_count(L, 1);
	check_param(L, PARAM_TYPE_STRING, 1);
	size_t len = 0;
	const char* pstr = lua_tolstring(L, 1, &len);
	if(len > max_len)
		len = max_len;
	const auto pduel = lua_get<duel*>(L);
	auto message = pduel->new_message(message_code);
	message->write<uint16_t>(static_cast<uint16_t>(len));
	message->write(pstr, len);
	message->write<uint8_t>(0);
	return 0;
}

LUA_FUNCTION_EXISTING(SetAIName, write_string_message<MSG_AI_NAME, 100>);
LUA_FUNCTION_EXISTING(ShowHint, write_string_message<MSG_SHOW_HINT, 1024>);

LUA_FUNCTION(PrintStacktrace) {
	interpreter::print_stacktrace(L);
	return 0;
}

LUA_FUNCTION(CardToStringWrapper) {
	const auto pcard = lua_get<card*>(L, 1);
	if(pcard) {
		luaL_checkstack(L, 4, nullptr);
		lua_getglobal(L, "Debug");
		lua_pushstring(L, "CardToString");
		lua_rawget(L, -2);
		if(!lua_isnil(L, -1)) {
			lua_pushvalue(L, 1);
			lua_call(L, 1, 1);
			return 1;
		}
		lua_settop(L, 1);
	}
	const char* kind = luaL_typename(L, 1);
	lua_pushfstring(L, "%s: %p", kind, lua_topointer(L, 1));
	return 1;
}

}

void scriptlib::push_debug_lib(lua_State* L) {
	static constexpr auto debuglib = GET_LUA_FUNCTIONS_ARRAY();
	static_assert(debuglib.back().name == nullptr, "");
	lua_createtable(L, 0, debuglib.size() - 1);
	luaL_setfuncs(L, debuglib.data(), 0);
	lua_setglobal(L, "Debug");
}
