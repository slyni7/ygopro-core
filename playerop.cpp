/*
 * Copyright (c) 2010-2015, Argon Sun (Fluorohydride)
 * Copyright (c) 2017-2024, Edoardo Lolletti (edo9300) <edoardo762@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma warning(disable: 4996)
#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <io.h>
#include <algorithm> //std::sort, std::unique
#include <iterator> //std::distance
#include <type_traits> //std::is_same
#include <stack>
#include <vector>
#include "bit.h"
#include "card.h"
#include "duel.h"
#include "effect.h"
#include "field.h"

bool field::process(Processors::SelectBattleCmd& arg) {
	auto playerid = arg.playerid;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if (arg.step == 0) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_battle_command : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%d,", 0, core.select_chains.size());
		fprintf(fp, "%d,%d,", 1, core.attackable_cards.size());
		fprintf(fp, "%d,%d,", 2, core.to_m2);
		fprintf(fp, "%d,%d", 3, core.to_ep);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 2) {
			int currval1 = currvals[0];
			int currval2 = currvals[1];
			core.select_chains.sort(chain::chain_operation_sort);
			returns.clear();
			returns.set<int32_t>(0, currval1 | (currval2 << 16));
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_battle_command : %d,%d", currval1, currval2);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currval1, currval2;
					char curr[25];
					sscanf(line, "%s : %d,%d", curr, &currval1, &currval2);
					if (!strcmp(curr, "select_battle_command")) {
						core.select_chains.sort(chain::chain_operation_sort);
						returns.clear();
						returns.set<int32_t>(0, currval1 | (currval2 << 16));
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currval1, currval2;
				char curr[25];
				sscanf(line, "%s : %d,%d", curr, &currval1, &currval2);
				if (!strcmp(curr, "select_battle_command")) {
					core.select_chains.sort(chain::chain_operation_sort);
					returns.clear();
					returns.set<int32_t>(0, currval1 | (currval2 << 16));
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		auto message = pduel->new_message(MSG_SELECT_BATTLECMD);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		//Activatable
		message->write<uint32_t>(core.select_chains.size());
		core.select_chains.sort(chain::chain_operation_sort);
		for(const auto& ch : core.select_chains) {
			effect* peffect = ch.triggering_effect;
			card* pcard = peffect->get_handler();
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
			message->write<uint64_t>(peffect->description);
			message->write<uint8_t>(peffect->get_client_mode());
		}
		//Attackable
		message->write<uint32_t>(core.attackable_cards.size());
		for (auto& pcard : core.attackable_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint8_t>(pcard->current.sequence);
			message->write<uint8_t>(pcard->direct_attackable);
		}
		//M2, EP
		if (core.to_m2)
			message->write<uint8_t>(1);
		else
			message->write<uint8_t>(0);
		if (core.to_ep)
			message->write<uint8_t>(1);
		else
			message->write<uint8_t>(0);
		return FALSE;
	}
	else {
		uint32_t t = returns.at<int32_t>(0) & 0xffff;
		uint32_t s = returns.at<int32_t>(0) >> 16;
		if (t > 3
			|| (t == 0 && s >= core.select_chains.size())
			|| (t == 1 && s >= core.attackable_cards.size())
			|| (t == 2 && !core.to_m2)
			|| (t == 3 && !core.to_ep)) {
			/*char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_battle_command : %d,%d", t, s);
			fprintf(fp, "\n");
			fclose(fp);

				int btl_player = infos.turn_player;
				bool btlcmdchk = false;
				if (infos.btlcmd_player != PLAYER_NONE) {
					btlcmdchk = true;
					btl_player = infos.btlcmd_player;
				}
				for (auto& pcard : player[btl_player].list_grave) {
					if (!pcard)
						continue;
					if (!pcard->is_affected_by_effect(EFFECT_KYRIE_ELEISON))
						continue;
					if (!pcard->is_capable_attack_announce(btl_player))
						continue;
					returns.set<int32_t>(0, 1 | ((core.attackable_cards.size() - 1) << 16));
					return TRUE;
				}*/

			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_battle_command : %d,%d", t, s);
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectIdleCmd& arg) {
	auto playerid = arg.playerid;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if (arg.step == 0) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_idle_command : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%d,", 0, core.summonable_cards.size());
		fprintf(fp, "%d,%d,", 1, core.spsummonable_cards.size());
		fprintf(fp, "%d,%d,", 2, core.repositionable_cards.size());
		fprintf(fp, "%d,%d,", 3, core.msetable_cards.size());
		fprintf(fp, "%d,%d,", 4, core.ssetable_cards.size());
		fprintf(fp, "%d,%d,", 5, core.select_chains.size());
		fprintf(fp, "%d,%d,", 6, (infos.phase == PHASE_MAIN1 && core.to_bp));
		fprintf(fp, "%d,%d", 7, core.to_ep);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 2) {
			int currval1 = currvals[0];
			int currval2 = currvals[1];
			core.select_chains.sort(chain::chain_operation_sort);
			returns.clear();
			returns.set<int32_t>(0, currval1 | (currval2 << 16));
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_idle_command : %d,%d", currval1, currval2);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currval1, currval2;
					char curr[25];
					sscanf(line, "%s : %d,%d", curr, &currval1, &currval2);
					if (!strcmp(curr, "select_idle_command")) {
						core.select_chains.sort(chain::chain_operation_sort);
						if (currval1 > 7
							|| (currval1 == 0 && currval2 >= core.summonable_cards.size())
							|| (currval1 == 1 && currval2 >= core.spsummonable_cards.size())
							|| (currval1 == 2 && currval2 >= core.repositionable_cards.size())
							|| (currval1 == 3 && currval2 >= core.msetable_cards.size())
							|| (currval1 == 4 && currval2 >= core.ssetable_cards.size())
							|| (currval1 == 5 && currval2 >= core.select_chains.size())
							|| (currval1 == 6 && (infos.phase != PHASE_MAIN1 || !core.to_bp))
							|| (currval1 == 7 && !core.to_ep)) {
							
							fclose(fp);
							return FALSE;
						}
						returns.clear();
						returns.set<int32_t>(0, currval1 | (currval2 << 16));
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currval1, currval2;
				char curr[25];
				sscanf(line, "%s : %d,%d", curr, &currval1, &currval2);
				if (!strcmp(curr, "select_idle_command")) {
					core.select_chains.sort(chain::chain_operation_sort);
					if (currval1 > 7
						|| (currval1 == 0 && currval2 >= core.summonable_cards.size())
						|| (currval1 == 1 && currval2 >= core.spsummonable_cards.size())
						|| (currval1 == 2 && currval2 >= core.repositionable_cards.size())
						|| (currval1 == 3 && currval2 >= core.msetable_cards.size())
						|| (currval1 == 4 && currval2 >= core.ssetable_cards.size())
						|| (currval1 == 5 && currval2 >= core.select_chains.size())
						|| (currval1 == 6 && (infos.phase != PHASE_MAIN1 || !core.to_bp))
						|| (currval1 == 7 && !core.to_ep)) {
						
						fclose(fp);
						return FALSE;
					}
					returns.clear();
					returns.set<int32_t>(0, currval1 | (currval2 << 16));
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		auto message = pduel->new_message(MSG_SELECT_IDLECMD);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		//idle summon
		message->write<uint32_t>(core.summonable_cards.size());
		for (auto& pcard : core.summonable_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
		}
		//idle spsummon
		message->write<uint32_t>(core.spsummonable_cards.size());
		for (auto& pcard : core.spsummonable_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
		}
		//idle pos change
		message->write<uint32_t>(core.repositionable_cards.size());
		for (auto& pcard : core.repositionable_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint8_t>(pcard->current.sequence);
		}
		//idle mset
		message->write<uint32_t>(core.msetable_cards.size());
		for (auto& pcard : core.msetable_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
		}
		//idle sset
		message->write<uint32_t>(core.ssetable_cards.size());
		for (auto& pcard : core.ssetable_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
		}
		//idle activate
		message->write<uint32_t>(core.select_chains.size());
		core.select_chains.sort(chain::chain_operation_sort);
		for(const auto& ch : core.select_chains) {
			effect* peffect = ch.triggering_effect;
			card* pcard = peffect->get_handler();
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
			message->write<uint64_t>(peffect->description);
			message->write<uint8_t>(peffect->get_client_mode());
		}
		//To BP
		if (((infos.phase == PHASE_MAIN1 && core.to_bp) || ((infos.idlecmd_player != PLAYER_NONE) && is_player_affected_by_effect(infos.idlecmd_player, EFFECT_GENKAI_BATTLE))) && (infos.btlcmd_player == PLAYER_NONE))
			message->write<uint8_t>(1);
		else
			message->write<uint8_t>(0);
		if (core.to_ep || (infos.idlecmd_player != PLAYER_NONE))
			message->write<uint8_t>(1);
		else
			message->write<uint8_t>(0);
		if (infos.can_shuffle && player[playerid].list_hand.size() > 1)
			message->write<uint8_t>(1);
		else
			message->write<uint8_t>(0);
		return FALSE;
	}
	else {
		uint32_t t = returns.at<int32_t>(0) & 0xffff;
		uint32_t s = returns.at<int32_t>(0) >> 16;
		if (t > 8
			|| (t == 0 && s >= core.summonable_cards.size())
			|| (t == 1 && s >= core.spsummonable_cards.size())
			|| (t == 2 && s >= core.repositionable_cards.size())
			|| (t == 3 && s >= core.msetable_cards.size())
			|| (t == 4 && s >= core.ssetable_cards.size())
			|| (t == 5 && s >= core.select_chains.size())
			|| (t == 6 && !(((infos.phase == PHASE_MAIN1 && core.to_bp) || ((infos.idlecmd_player != PLAYER_NONE) && is_player_affected_by_effect(infos.idlecmd_player, EFFECT_GENKAI_BATTLE))) && (infos.btlcmd_player == PLAYER_NONE)))
			|| (t == 7 && !core.to_ep && (infos.idlecmd_player == PLAYER_NONE))
			|| (t == 8 && !(infos.can_shuffle && player[playerid].list_hand.size() > 1))) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_idle_command : %d,%d", t, s);
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectEffectYesNo& arg) {
	auto playerid = arg.playerid;
	auto pcard = arg.pcard;
	auto description = arg.description;
		char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_effect_yes_no : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d", 2);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.data.push_back(currvalue);
			returns.data.push_back(currvalue >> 8);
			returns.data.push_back(currvalue >> 16);
			returns.data.push_back(currvalue >> 24);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_effect_yes_no : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "select_effect_yes_no")) {
						returns.clear();
						returns.data.push_back(currvalue);
						returns.data.push_back(currvalue >> 8);
						returns.data.push_back(currvalue >> 16);
						returns.data.push_back(currvalue >> 24);
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "select_effect_yes_no")) {
					returns.clear();
					returns.data.push_back(currvalue);
					returns.data.push_back(currvalue >> 8);
					returns.data.push_back(currvalue >> 16);
					returns.data.push_back(currvalue >> 24);
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		if((arg.playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			returns.set<int32_t>(0, 1);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_EFFECTYN);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint32_t>(pcard->data.code);
		message->write(pcard->get_info_location());
		message->write<uint64_t>(description);
		returns.set<int32_t>(0, -1);
		return FALSE;
	}
	else {
		if (returns.at<int32_t>(0) != 0 && returns.at<int32_t>(0) != 1) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_effect_yes_no : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectYesNo& arg) {
	auto playerid = arg.playerid;
	auto description = arg.description;	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_yes_no : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d", 2);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.set<int32_t>(0, currvalue);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_yes_no : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "select_yes_no")) {
						returns.clear();
						returns.set<int32_t>(0, currvalue);
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "select_yes_no")) {
					returns.clear();
					returns.set<int32_t>(0, currvalue);
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		if((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			returns.set<int32_t>(0, 1);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_YESNO);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint64_t>(description);
		returns.set<int32_t>(0, -1);
		return FALSE;
	}
	else {
		if (returns.at<int32_t>(0) != 0 && returns.at<int32_t>(0) != 1) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_yes_no : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectOption& arg) {
	auto playerid = arg.playerid;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(core.select_options.size() == 0) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_option : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d", core.select_options.size());
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.data.push_back(currvalue);
			returns.data.push_back(currvalue >> 8);
			returns.data.push_back(currvalue >> 16);
			returns.data.push_back(currvalue >> 24);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_option : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "select_option")) {
						returns.clear();
						returns.data.push_back(currvalue);
						returns.data.push_back(currvalue >> 8);
						returns.data.push_back(currvalue >> 16);
						returns.data.push_back(currvalue >> 24);
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "select_option")) {
					returns.clear();
					returns.data.push_back(currvalue);
					returns.data.push_back(currvalue >> 8);
					returns.data.push_back(currvalue >> 16);
					returns.data.push_back(currvalue >> 24);
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		returns.set<int32_t>(0, -1);
		if (core.select_options.size() == 0) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			returns.set<int32_t>(0, 0);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_OPTION);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(static_cast<uint8_t>(core.select_options.size()));
		for (auto& option : core.select_options)
			message->write<uint64_t>(option);
		return FALSE;
	}
	else {
		if (returns.at<int32_t>(0) < 0 || returns.at<int32_t>(0) >= (int32_t)core.select_options.size()) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_option : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}

namespace {
	template<typename ReturnType>
	bool parse_response_cards(ProgressiveBuffer& returns, return_card_generic<ReturnType>& return_cards, const std::vector<ReturnType>& select_cards, bool cancelable) {
		auto type = returns.at<int32_t>(0);
		//allowed values are from -1 to 3
		if (static_cast<uint32_t>(type + 1) > 4)
			return false;
		if (type == -1) {
			if (cancelable) {
				return_cards.canceled = true;
				return true;
			}
			return false;
		}
		auto& list = return_cards.list;
		if (type == 3) {
			for (size_t i = 0; i < select_cards.size(); ++i) {
				if (returns.bitGet(i + (sizeof(uint32_t) * 8)))
					list.push_back(select_cards[i]);
			}
		}
		else {
			auto size = returns.at<uint32_t>(1);
			for (uint32_t i = 0; i < size; ++i) {
				auto idx = (type == 0) ? returns.at<uint32_t>(i + 2) :
					(type == 1) ? returns.at<uint16_t>(i + 4) :
					returns.at<uint8_t>(i + 8);
				if (idx >= select_cards.size())
					return false;
				list.push_back(select_cards[idx]);
			}
		}
		if (std::is_same_v<ReturnType, card*>) {
			std::sort(list.begin(), list.end());
			auto ip = std::unique(list.begin(), list.end());
			bool res = (ip == list.end());
			list.resize(std::distance(list.begin(), ip));
			return res;
		}
		return true;
	}
}
bool inline field::parse_response_cards(bool cancelable) {
	return ::parse_response_cards(returns, return_cards, core.select_cards, cancelable);
}
bool field::process(Processors::SelectCard& arg) {
	auto playerid = arg.playerid;
	auto cancelable = arg.cancelable;
	auto min = arg.min;
	auto max = arg.max;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(max == 0 || core.select_cards.empty()) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_card : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%d", min, max);
		int cs = core.select_cards.size();
		if (cancelable)
			cs = -cs;
		fprintf(fp, ",%d", cs);
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		for (auto& pcard : core.select_cards) {
			fprintf(fp, ",%d", pcard->fieldid_r);
		}
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<int32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li >= 1) {
			return_cards.clear();
			returns.clear();
			if (currvals.size() && (currvals.back() == -1)) {
				return_cards.canceled = true;
				if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
					char fc[50];
					if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
					FILE *fp = NULL;
					fopen_s(&fp, fc, "a+");
					bool first = true;
					fprintf(fp, "select_card : -1");
					fprintf(fp, "\n");
					fclose(fp);
				}
				return TRUE;
			}
			for (auto& pcard : core.select_cards) {
				for (auto currval : currvals) {
					if (pcard->fieldid_r == currval) {
						return_cards.list.push_back(pcard);
					}
				}
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_card : ");
				for (auto& pcard : return_cards.list) {
					if (first) {
						first = false;
						fprintf(fp, "%d", pcard->fieldid_r);
					}
					else
						fprintf(fp, ",%d", pcard->fieldid_r);
				}
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					char curr[25];
					char currstr[400];
					sscanf(line, "%s : %s", curr, currstr);
					std::vector<int32_t> currvals;
					currvals.push_back(0);
					for (int s = 0; s < strlen(currstr); s++) {
						char nowc = currstr[s];
						if (nowc == 0x2d) {
							currvals.back() = -1;
							break;
						}
						if (nowc == 0x2c) {
							currvals.push_back(0);
						}
						if ((nowc >= 0x30) && (nowc <= 0x39)) {
							currvals.back() = currvals.back() * 10 + (nowc - 0x30);
						}
					}
					if (!strcmp(curr, "select_card")) {
						return_cards.clear();
						returns.clear();
						if (currvals.back() == -1) {
							return_cards.canceled = true;
						}
						else {
							for (auto& pcard : core.select_cards) {
								for (auto currval : currvals) {
									if (pcard->fieldid_r == currval) {
										return_cards.list.push_back(pcard);
									}
								}
							}
						}
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				char curr[25];
				char currstr[400];
				sscanf(line, "%s : %s", curr, currstr);
				std::vector<int32_t> currvals;
				currvals.push_back(0);
				for (int s = 0; s < strlen(currstr); s++) {
					char nowc = currstr[s];
					if (nowc == 0x2d) {
						currvals.back() = -1;
						break;
					}
					if (nowc == 0x2c) {
						currvals.push_back(0);
					}
					if ((nowc >= 0x30) && (nowc <= 0x39)) {
						currvals.back() = currvals.back() * 10 + (nowc - 0x30);
					}
				}
				if (!strcmp(curr, "select_card")) {
					return_cards.clear();
					returns.clear();
					if (currvals.back() == -1) {
						return_cards.canceled = true;
					}
					else {
						for (auto& pcard : core.select_cards) {
							for (auto currval : currvals) {
								if (pcard->fieldid_r == currval) {
									return_cards.list.push_back(pcard);
								}
							}
						}
					}
					fclose(fp);
					pduel->playerop_line++;
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		return_cards.clear();
		returns.clear();
		if (max == 0 || core.select_cards.empty()) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		if (max > core.select_cards.size())
			max = static_cast<uint8_t>(core.select_cards.size());
		if (min > max)
			min = max;
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			for (int32_t i = 0; i < min; ++i) {
				return_cards.list.push_back(core.select_cards[i]);
			}
			return TRUE;
		}
		arg.min = min;
		arg.max = max;
		auto message = pduel->new_message(MSG_SELECT_CARD);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(cancelable || min == 0);
		message->write<uint32_t>(min);
		message->write<uint32_t>(max);
		message->write<uint32_t>((uint32_t)core.select_cards.size());
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		for (auto& pcard : core.select_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write(pcard->get_info_location());
		}
		return FALSE;
	}
	else {
		if (!parse_response_cards(cancelable || min == 0)) {
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if (return_cards.canceled) {
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_card : -1");
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
		if (return_cards.list.size() < min || return_cards.list.size() > max) {
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "select_card : ");
			for (auto& pcard : return_cards.list) {
				if (first) {
					first = false;
					fprintf(fp, "%d", pcard->fieldid_r);
				}
				else
					fprintf(fp, ",%d", pcard->fieldid_r);
			}
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectCardCodes& arg) {
	auto playerid = arg.playerid;
	auto cancelable = arg.cancelable;
	auto min = arg.min;
	auto max = arg.max;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(max == 0 || core.select_cards_codes.empty()) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_card_codes : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%d", min, max);
		fprintf(fp, ",%d", core.select_cards_codes.size());
		for (const auto& obj : core.select_cards_codes) {
			fprintf(fp, ",%d", obj.first);
		}
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li >= 1) {
			return_card_codes.clear();
			returns.clear();
			for (auto& pcard : core.select_cards) {
				for (auto currval : currvals) {
					if (pcard->fieldid_r == currval) {
						return_cards.list.push_back(pcard);
					}
				}
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_card_codes : ");
				for (auto& currval : currvals) {
					if (first) {
						first = false;
						fprintf(fp, "%d", currval);
					}
					else
						fprintf(fp, ",%d", currval);
				}
				fprintf(fp, "\n");
				fclose(fp);
			}
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					char curr[25];
					char currstr[400];
					sscanf(line, "%s : %s", curr, currstr);
					std::vector<uint32_t> currvals;
					currvals.push_back(0);
					for (int s = 0; s < strlen(currstr); s++) {
						char nowc = currstr[s];
						if (nowc == 0x2d) {
							pduel->playerop_line++;
							fclose(fp);
							return TRUE;
						}
						if (nowc == 0x2c) {
							currvals.push_back(0);
						}
						if ((nowc >= 0x30) && (nowc <= 0x39)) {
							currvals.back() = currvals.back() * 10 + (nowc - 0x30);
						}
					}
					if (!strcmp(curr, "select_card_coes")) {
						return_card_codes.clear();
						returns.clear();
						int pairfirst = 0;
						for (auto currval : currvals) {
							if (!pairfirst)
								pairfirst = currval;
							else {
								return_card_codes.list.push_back(std::pair<uint32_t, uint32_t>(pairfirst, currval));
								pairfirst = 0;
							}
						}
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				char curr[25];
				char currstr[400];
				sscanf(line, "%s : %s", curr, currstr);
				std::vector<uint32_t> currvals;
				currvals.push_back(0);
				for (int s = 0; s < strlen(currstr); s++) {
					char nowc = currstr[s];
					if (nowc == 0x2d) {
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
					if (nowc == 0x2c) {
						currvals.push_back(0);
					}
					if ((nowc >= 0x30) && (nowc <= 0x39)) {
						currvals.back() = currvals.back() * 10 + (nowc - 0x30);
					}
				}
				if (!strcmp(curr, "select_card_coes")) {
					return_card_codes.clear();
					returns.clear();
					int pairfirst = 0;
					for (auto currval : currvals) {
						if (!pairfirst)
							pairfirst = currval;
						else {
							return_card_codes.list.push_back(std::pair<uint32_t, uint32_t>(pairfirst, currval));
							pairfirst = 0;
						}
					}
					fclose(fp);
					pduel->playerop_line++;
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		return_card_codes.clear();
		returns.clear();
		if (max == 0 || core.select_cards_codes.empty()) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		if (max > core.select_cards_codes.size())
			max = static_cast<uint8_t>(core.select_cards_codes.size());
		if (min > max)
			min = max;
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			for (int32_t i = 0; i < min; ++i) {
				return_card_codes.list.push_back(core.select_cards_codes[i]);
			}
			return TRUE;
		}
		arg.min = min;
		arg.max = max;
		auto message = pduel->new_message(MSG_SELECT_CARD);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(cancelable || min == 0);
		message->write<uint32_t>(min);
		message->write<uint32_t>(max);
		message->write<uint32_t>((uint32_t)core.select_cards_codes.size());
		for (const auto& obj : core.select_cards_codes) {
			message->write<uint32_t>(obj.first);
			message->write(loc_info{ playerid, 0, 0, 0 });
		}
		return FALSE;
	}
	else {
		if (!::parse_response_cards(returns, return_card_codes, core.select_cards_codes, cancelable || min == 0)) {
			return_card_codes.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if (return_card_codes.canceled) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "select_card_codes : -1");
			fprintf(fp, "\n");

			fclose(fp);
			return TRUE;
		}
		if (return_card_codes.list.size() < min || return_card_codes.list.size() > max) {
			return_card_codes.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "select_card_codes : ");
			for (auto& pccode : return_card_codes.list) {
				if (first) {
					first = false;
					fprintf(fp, "%d,%d", pccode.first, pccode.second);
				}
				else
					fprintf(fp, ",%d,%d", pccode.first, pccode.second);
			}
			fprintf(fp, "\n");

			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectUnselectCard& arg) {
	auto playerid = arg.playerid;
	auto cancelable = arg.cancelable;
	auto min = arg.min;
	auto max = arg.max;
	auto finishable = arg.finishable;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(core.select_cards.empty() && core.unselect_cards.empty()) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_unselect_card : ");
		int32_t _max = (int32_t)(core.select_cards.size() + core.unselect_cards.size());
		fprintf(fp, "%d,", playerid);
		int cs = core.select_cards.size();
		int us = core.unselect_cards.size();
		if (cancelable) {
			cs = -cs;
			us = -us;
		}
		fprintf(fp, "%d,%d", cs, us);
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			return_cards.clear();
			returns.clear();
			if (currvalue == -1) {
				return_cards.canceled = true;
			}
			else {
				std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
				return_cards.list.push_back((currvalue >= (int32_t)core.select_cards.size()) ? core.unselect_cards[currvalue - core.select_cards.size()] : core.select_cards[currvalue]);
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_unselect_card : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "select_unselect_card")) {
						return_cards.clear();
						returns.clear();
						if (currvalue == -1) {
							return_cards.canceled = true;
							pduel->playerop_line++;
							fclose(fp);
							return TRUE;
						}
						std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
						return_cards.list.push_back((currvalue >= (int32_t)core.select_cards.size()) ? core.unselect_cards[currvalue - core.select_cards.size()] : core.select_cards[currvalue]);
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "select_unselect_card")) {
					return_cards.clear();
					returns.clear();
					if (currvalue == -1) {
						return_cards.canceled = true;
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
					std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
					return_cards.list.push_back((currvalue >= (int32_t)core.select_cards.size()) ? core.unselect_cards[currvalue - core.select_cards.size()] : core.select_cards[currvalue]);
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if (arg.step == 0) {
		return_cards.clear();
		returns.clear();
		if (core.select_cards.empty() && core.unselect_cards.empty()) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			if (cancelable)
				return_cards.canceled = true;
			else
				return_cards.list.push_back(core.select_cards.size() ? core.select_cards.front() : core.unselect_cards.front());
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_UNSELECT_CARD);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(finishable);
		message->write<uint8_t>(cancelable);
		message->write<uint32_t>(min);
		message->write<uint32_t>(max);
		message->write<uint32_t>((uint32_t)core.select_cards.size());
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		for (auto& pcard : core.select_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write(pcard->get_info_location());
		}
		message->write<uint32_t>((uint32_t)core.unselect_cards.size());
		for (auto& pcard : core.unselect_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write(pcard->get_info_location());
		}
		return FALSE;
	}
	else {
		if (returns.at<int32_t>(0) == -1) {
			if (cancelable || finishable) {
				return_cards.canceled = true;
				if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
					char fc[50];
					if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
					FILE *fp = NULL;
					fopen_s(&fp, fc, "a+");
					fprintf(fp, "select_unselect_card : %d", -1);
					fprintf(fp, "\n");
					fclose(fp);
				}
				return TRUE;
			}
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if (returns.at<int32_t>(0) == 0 || returns.at<int32_t>(0) > 1) {
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		int32_t _max = (int32_t)(core.select_cards.size() + core.unselect_cards.size());
		int retval = returns.at<int32_t>(1);
		if (retval < 0 || retval >= _max) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		return_cards.list.push_back((retval >= (int32_t)core.select_cards.size()) ? core.unselect_cards[retval - core.select_cards.size()] : core.select_cards[retval]);
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_unselect_card : %d", returns.at<int32_t>(1));
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectChain& arg) {
	auto playerid = arg.playerid;
	auto spe_count = arg.spe_count;
	auto forced = arg.forced;	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_chain : ");
		fprintf(fp, "%d,", playerid);
		int cs = core.select_chains.size();
		if (!forced)
			cs = -cs;
		fprintf(fp, "%d", cs);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			core.select_chains.sort(chain::chain_operation_sort);
			returns.clear();
			returns.set<int32_t>(0, currvalue);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_chain : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "select_chain")) {
						core.select_chains.sort(chain::chain_operation_sort);
						if ((currvalue < -1) || (forced && currvalue < 0) || currvalue >= (int32_t)core.select_chains.size()) {
							
							fclose(fp);
							return FALSE;
						}
						returns.clear();
						returns.set<int32_t>(0, currvalue);
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "select_chain")) {
					core.select_chains.sort(chain::chain_operation_sort);
					if ((currvalue < -1) || (forced && currvalue < 0) || currvalue >= (int32_t)core.select_chains.size()) {
						
						fclose(fp);
						return FALSE;
					}
					returns.clear();
					returns.set<int32_t>(0, currvalue);
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		returns.set<int32_t>(0, -1);
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			if (core.select_chains.size() == 0)
				returns.set<int32_t>(0, -1);
			else if (forced)
				returns.set<int32_t>(0, 0);
			else {
				bool act = true;
				for (const auto& ch : core.current_chain)
					if (ch.triggering_player == 1)
						act = false;
				if (act)
					returns.set<int32_t>(0, 0);
				else
					returns.set<int32_t>(0, -1);
			}
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_CHAIN);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(spe_count);
		message->write<uint8_t>(forced);
		message->write<uint32_t>(core.hint_timing[playerid]);
		message->write<uint32_t>(core.hint_timing[1 - playerid]);
		core.select_chains.sort(chain::chain_operation_sort);
		message->write<uint32_t>(core.select_chains.size());
		for (const auto& ch : core.select_chains) {
			effect* peffect = ch.triggering_effect;
			card* pcard = peffect->get_handler();
			message->write<uint32_t>(pcard->data.code);
			message->write(pcard->get_info_location());
			message->write<uint64_t>(peffect->description);
			message->write<uint8_t>(peffect->get_client_mode() || !pcard->current.location);
		}
		return FALSE;
	}
	else {
		if (!forced && returns.at<int32_t>(0) == -1) {
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_chain : %d", returns.at<int32_t>(0));
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
		if (returns.at<int32_t>(0) < 0 || returns.at<int32_t>(0) >= (int32_t)core.select_chains.size()) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_chain : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");

			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectPlace& arg) {
	auto playerid = arg.playerid;
	auto flag = arg.flag;
	auto count = arg.count;
	auto disable_field = arg.disable_field;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(count == 0) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_place : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%d", count, flag);
		fprintf(fp, "\n");
		fclose(fp); 
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 3) {
			returns.clear();
			for (auto currval : currvals)
				returns.data.push_back(currval);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_place : ");
				for (auto& currval : currvals) {
					if (first) {
						first = false;
						fprintf(fp, "%d", currval);
					}
					else
						fprintf(fp, ",%d", currval);
				}
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					char curr[25];
					char currstr[400];
					sscanf(line, "%s : %s", curr, currstr);
					std::vector<uint8_t> currvals;
					currvals.push_back(0);
					for (int s = 0; s < strlen(currstr); s++) {
						char nowc = currstr[s];
						if (nowc == 0x2c) {
							currvals.push_back(0);
						}
						if ((nowc >= 0x30) && (nowc <= 0x39)) {
							currvals.back() = currvals.back() * 10 + (nowc - 0x30);
						}
					}
					if (!strcmp(curr, "select_place")) {
						returns.clear();
						for (auto currval : currvals)
							returns.data.push_back(currval);
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				char curr[25];
				char currstr[400];
				sscanf(line, "%s : %s", curr, currstr);
				std::vector<uint8_t> currvals;
				currvals.push_back(0);
				for (int s = 0; s < strlen(currstr); s++) {
					char nowc = currstr[s];
					if (nowc == 0x2c) {
						currvals.push_back(0);
					}
					if ((nowc >= 0x30) && (nowc <= 0x39)) {
						currvals.back() = currvals.back() * 10 + (nowc - 0x30);
					}
				}
				if (!strcmp(curr, "select_place")) {
					returns.clear();
					for (auto currval : currvals)
						returns.data.push_back(currval);
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		if(count == 0) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			flag = ~flag;
			int32_t filter;
			int32_t pzone = 0;
			if (flag & 0x7f) {
				returns.set<int8_t>(0, 1);
				returns.set<int8_t>(1, LOCATION_MZONE);
				filter = flag & 0x7f;
			}
			else if (flag & 0x1f00) {
				returns.set<int8_t>(0, 1);
				returns.set<int8_t>(1, LOCATION_SZONE);
				filter = (flag >> 8) & 0x1f;
			}
			else if (flag & 0xc000) {
				returns.set<int8_t>(0, 1);
				returns.set<int8_t>(1, LOCATION_SZONE);
				filter = (flag >> 14) & 0x3;
				pzone = 1;
			}
			else if (flag & 0x7f0000) {
				returns.set<int8_t>(0, 0);
				returns.set<int8_t>(1, LOCATION_MZONE);
				filter = (flag >> 16) & 0x7f;
			}
			else if (flag & 0x1f000000) {
				returns.set<int8_t>(0, 0);
				returns.set<int8_t>(1, LOCATION_SZONE);
				filter = (flag >> 24) & 0x1f;
			}
			else {
				returns.set<int8_t>(0, 0);
				returns.set<int8_t>(1, LOCATION_SZONE);
				filter = (flag >> 30) & 0x3;
				pzone = 1;
			}
			if (!pzone) {
				if (filter & 0x40) returns.set<int8_t>(2, 6);
				else if (filter & 0x20) returns.set<int8_t>(2, 5);
				else if (filter & 0x4) returns.set<int8_t>(2, 2);
				else if (filter & 0x2) returns.set<int8_t>(2, 1);
				else if (filter & 0x8) returns.set<int8_t>(2, 3);
				else if (filter & 0x1) returns.set<int8_t>(2, 0);
				else if (filter & 0x10) returns.set<int8_t>(2, 4);
			}
			else {
				if (filter & 0x1) returns.set<int8_t>(2, 6);
				else if (filter & 0x2) returns.set<int8_t>(2, 7);
			}
			return TRUE;
		}
		auto message = pduel->new_message(disable_field ? MSG_SELECT_DISFIELD : MSG_SELECT_PLACE);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(count);
		if (eset.size())
			message->write<uint32_t>((flag >> 16) | ((flag & 0xffff) << 16));
		else
			message->write<uint32_t>(flag);
		returns.set<int8_t>(0, 0);
		return FALSE;
	}
	else {
		auto retry = [&pduel = pduel]() {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		};
		uint8_t pt = 0;
		for (int8_t i = 0; i < count; ++i) {
			uint8_t select_player = returns.at<uint8_t>(pt);
			if (select_player > 1)
				return retry();
			const bool isplayerid = (select_player == playerid);
			uint8_t location = returns.at<uint8_t>(pt + 1);
			if (location != LOCATION_MZONE && location != LOCATION_SZONE)
				return retry();
			const bool ismzone = location == LOCATION_MZONE;
			uint8_t sequence = returns.at<uint8_t>(pt + 2);
			if (sequence > 7 - ismzone) //szone allow max of 8 zones, so 0-7, mzones 7 zones, so 0-6
				return retry();
			uint32_t to_check = 0x1u << sequence;
			if (!ismzone)
				to_check <<= 8;
			if (!isplayerid)
				to_check <<= 16;
			if (to_check & flag)
				return retry();
			flag |= to_check;
			pt += 3;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "select_place : ");
			pt = 0;
			for (int8_t i = 0; i < count; ++i) {
				if (first)
					fprintf(fp, "%d,%d,%d", returns.at<uint8_t>(pt), returns.at<uint8_t>(pt + 1), returns.at<uint8_t>(pt + 2));
				else
					fprintf(fp, ",%d,%d,%d", returns.at<uint8_t>(pt), returns.at<uint8_t>(pt + 1), returns.at<uint8_t>(pt + 2));
				pt += 3;
			}
			fprintf(fp, "\n");
			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectPosition& arg) {
	auto playerid = arg.playerid;
	auto code = arg.code;
	uint8_t positions = arg.positions;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(positions == 0)
		&& !(positions == 0x1 || positions == 0x2 || positions == 0x4 || positions == 0x8)
		&& !((playerid == 1) && is_flag(DUEL_SIMPLE_AI))) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_position : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d", positions);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.data.push_back(currvalue);
			returns.data.push_back(currvalue >> 8);
			returns.data.push_back(currvalue >> 16);
			returns.data.push_back(currvalue >> 24);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "select_position : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "select_position")) {
						returns.clear();
						returns.data.push_back(currvalue);
						returns.data.push_back(currvalue >> 8);
						returns.data.push_back(currvalue >> 16);
						returns.data.push_back(currvalue >> 24);
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "select_position")) {
					returns.clear();
					returns.data.push_back(currvalue);
					returns.data.push_back(currvalue >> 8);
					returns.data.push_back(currvalue >> 16);
					returns.data.push_back(currvalue >> 24);
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		if(positions == 0) {
			returns.set<int32_t>(0, POS_FACEUP_ATTACK);
			return TRUE;
		}
		positions &= 0xf;
		if (positions == 0x1 || positions == 0x2 || positions == 0x4 || positions == 0x8) {
			returns.set<int32_t>(0, positions);
			return TRUE;
		}
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			if (positions & 0x4)
				returns.set<int32_t>(0, 0x4);
			else if (positions & 0x1)
				returns.set<int32_t>(0, 0x1);
			else if (positions & 0x8)
				returns.set<int32_t>(0, 0x8);
			else
				returns.set<int32_t>(0, 0x2);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_POSITION);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint32_t>(code);
		message->write<uint8_t>(positions);
		returns.set<int32_t>(0, 0);
		return FALSE;
	}
	else {
		uint32_t pos = returns.at<int32_t>(0);
		if ((pos != 0x1 && pos != 0x2 && pos != 0x4 && pos != 0x8) || !(pos & positions)) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "select_position : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");

			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectTributeP& arg) {
	auto playerid = arg.playerid;
	auto cancelable = arg.cancelable;
	auto min = arg.min;
	auto max = arg.max;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(max == 0 || core.select_cards.empty())) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_tribute : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%d", min, max);
		int cs = core.select_cards.size();
		if (cancelable)
			cs = -cs;
		fprintf(fp, ",%d", cs);
		for (auto& pcard : core.select_cards) {
			fprintf(fp, ",%d", pcard->fieldid_r);
		}
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li >= 1) {
			return_cards.clear();
			returns.clear();
			for (auto& pcard : core.select_cards) {
				for (auto currval : currvals) {
					if (pcard->fieldid_r == currval) {
						return_cards.list.push_back(pcard);
					}
				}
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_tribute : ");
				for (auto& pcard : return_cards.list) {
					if (first) {
						first = false;
						fprintf(fp, "%d", pcard->fieldid_r);
					}
					else
						fprintf(fp, ",%d", pcard->fieldid_r);
				}
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if (((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) && !(max == 0 || core.select_cards.empty())) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					char curr[25];
					char currstr[400];
					sscanf(line, "%s : %s", curr, currstr);
					std::vector<uint32_t> currvals;
					currvals.push_back(0);
					for (int s = 0; s < strlen(currstr); s++) {
						char nowc = currstr[s];
						if (nowc == 0x2d) {
							fclose(fp);
							pduel->playerop_line++;
							return TRUE;
						}
						if (nowc == 0x2c) {
							currvals.push_back(0);
						}
						if ((nowc >= 0x30) && (nowc <= 0x39)) {
							currvals.back() = currvals.back() * 10 + (nowc - 0x30);
						}
					}
					if (!strcmp(curr, "select_tribute")) {
						return_cards.clear();
						returns.clear();
						for (auto& pcard : core.select_cards) {
							for (auto currval : currvals) {
								if (pcard->fieldid_r == currval) {
									return_cards.list.push_back(pcard);
								}
							}
						}
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				char curr[25];
				char currstr[400];
				sscanf(line, "%s : %s", curr, currstr);
				std::vector<uint32_t> currvals;
				currvals.push_back(0);
				for (int s = 0; s < strlen(currstr); s++) {
					char nowc = currstr[s];
					if (nowc == 0x2d) {
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
					if (nowc == 0x2c) {
						currvals.push_back(0);
					}
					if ((nowc >= 0x30) && (nowc <= 0x39)) {
						currvals.back() = currvals.back() * 10 + (nowc - 0x30);
					}
				}
				if (!strcmp(curr, "select_tribute")) {
					return_cards.clear();
					returns.clear();
					for (auto& pcard : core.select_cards) {
						for (auto currval : currvals) {
							if (pcard->fieldid_r == currval) {
								return_cards.list.push_back(pcard);
							}
						}
					}
					fclose(fp);
					pduel->playerop_line++;
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		returns.clear();
		return_cards.clear();
		if (max == 0 || core.select_cards.empty()) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		uint8_t tm = 0;
		for (auto& pcard : core.select_cards)
			tm += pcard->release_param;
		if (max > 5)
			max = 5;
		if (max > tm)
			max = tm;
		if (min > max)
			min = max;
		arg.min = min;
		arg.max = max;
		auto message = pduel->new_message(MSG_SELECT_TRIBUTE);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(cancelable || min == 0);
		message->write<uint32_t>(min);
		message->write<uint32_t>(max);
		message->write<uint32_t>((uint32_t)core.select_cards.size());
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		for (auto& pcard : core.select_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
			message->write<uint8_t>(pcard->release_param);
		}
		return FALSE;
	}
	else {
		if (!parse_response_cards(cancelable || min == 0)) {
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if (return_cards.canceled) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "select_tribute : -1");
			fprintf(fp, "\n");

			fclose(fp);
			return TRUE;
		}
		if (return_cards.list.size() > max) {
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		int32_t tt = 0;
		for (auto& pcard : return_cards.list)
			tt += pcard->release_param;
		if (tt < min) {
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "select_tribute : ");
			for (auto& pcard : return_cards.list) {
				if (first) {
					first = false;
					fprintf(fp, "%d", pcard->fieldid_r);
				}
				else
					fprintf(fp, ",%d", pcard->fieldid_r);
			}
			fprintf(fp, "\n");

			fclose(fp);
		}
		return TRUE;
	}
}
bool field::process(Processors::SelectCounter& arg) {
	auto playerid = arg.playerid;
	auto countertype = arg.countertype;
	auto count = arg.count;
	auto self = arg.self;
	auto oppo = arg.oppo;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(count == 0)) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_counter : ");
		fprintf(fp, "%d,", playerid);
		uint8_t avail = self;
		uint8_t pid = playerid;
		uint32_t total = 0;
		core.select_cards.clear();
		for (int p = 0; p < 2; ++p) {
			if (avail) {
				for (auto& pcard : player[pid].list_mzone) {
					if (pcard && pcard->get_counter(countertype)) {
						core.select_cards.push_back(pcard);
						total += pcard->get_counter(countertype);
					}
				}
				for (auto& pcard : player[pid].list_szone) {
					if (pcard && pcard->get_counter(countertype)) {
						core.select_cards.push_back(pcard);
						total += pcard->get_counter(countertype);
					}
				}
			}
			pid = 1 - pid;
			avail = oppo;
		}
		if (count > total)
			count = total;
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		fprintf(fp, "%d", count);
		fprintf(fp, ",%d", core.select_cards.size());
		for (auto& pcard : core.select_cards) {
			fprintf(fp, ",%d", pcard->get_counter(countertype));
		}
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_counter : ");
				for (uint32_t i = 0; i < core.select_cards.size(); ++i) {
					if (first) {
						first = false;
						fprintf(fp, "%d", returns.at<int16_t>(i));
					}
					else
						fprintf(fp, ",%d", returns.at<int16_t>(i));
				}
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					char curr[25];
					char currstr[400];
					sscanf(line, "%s : %s", curr, currstr);
					std::vector<uint32_t> currvals;
					currvals.push_back(0);
					for (int s = 0; s < strlen(currstr); s++) {
						char nowc = currstr[s];
						if (nowc == 0x2d) {
							fclose(fp);
							pduel->playerop_line++;
							return TRUE;
						}
						if (nowc == 0x2c) {
							currvals.push_back(0);
						}
						if ((nowc >= 0x30) && (nowc <= 0x39)) {
							currvals.back() = currvals.back() * 10 + (nowc - 0x30);
						}
					}
					if (!strcmp(curr, "select_counter")) {
						int index = 0;
						for (auto currval : currvals) {
							returns.set<int32_t>(index, currval);
							index++;
						}
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				char curr[25];
				char currstr[400];
				sscanf(line, "%s : %s", curr, currstr);
				std::vector<uint32_t> currvals;
				currvals.push_back(0);
				for (int s = 0; s < strlen(currstr); s++) {
					char nowc = currstr[s];
					if (nowc == 0x2d) {
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
					if (nowc == 0x2c) {
						currvals.push_back(0);
					}
					if ((nowc >= 0x30) && (nowc <= 0x39)) {
						currvals.back() = currvals.back() * 10 + (nowc - 0x30);
					}
				}
				if (!strcmp(curr, "select_counter")) {
					int index = 0;
					for (auto currval : currvals) {
						returns.set<int32_t>(index, currval);
						index++;
					}
					fclose(fp);
					pduel->playerop_line++;
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}

	if(arg.step == 0) {
		if(count == 0)
			return TRUE;
		uint8_t avail = self;
		uint8_t fp = playerid;
		uint32_t total = 0;
		core.select_cards.clear();
		for (int p = 0; p < 2; ++p) {
			if (avail) {
				for (auto& pcard : player[fp].list_mzone) {
					if (pcard && pcard->get_counter(countertype)) {
						core.select_cards.push_back(pcard);
						total += pcard->get_counter(countertype);
					}
				}
				for (auto& pcard : player[fp].list_szone) {
					if (pcard && pcard->get_counter(countertype)) {
						core.select_cards.push_back(pcard);
						total += pcard->get_counter(countertype);
					}
				}
			}
			fp = 1 - fp;
			avail = oppo;
		}
		if (count > total)
			count = total;
		if(core.select_cards.size() == 1) {
			returns.set<int16_t>(0, count);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_COUNTER);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint16_t>(countertype);
		message->write<uint16_t>(count);
		message->write<uint32_t>(core.select_cards.size());
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		for (auto& pcard : core.select_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint8_t>(pcard->current.location);
			message->write<uint8_t>(pcard->current.sequence);
			message->write<uint16_t>(pcard->get_counter(countertype));
		}
		return FALSE;
	}
	else {
		uint16_t ct = 0;
		for (uint32_t i = 0; i < core.select_cards.size(); ++i) {
			if (core.select_cards[i]->get_counter(countertype) < returns.at<int16_t>(i)) {
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			ct += returns.at<int16_t>(i);
		}
		if (ct != count) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "select_counter : ");
			for (uint32_t i = 0; i < core.select_cards.size(); ++i) {
				if (first) {
					first = false;
					fprintf(fp, "%d", returns.at<int16_t>(i));
				}
				else
					fprintf(fp, ",%d", returns.at<int16_t>(i));
			}
			fprintf(fp, "\n");
			fclose(fp);
		}
	}
	return TRUE;
}
static int32_t select_sum_check1(const std::vector<int32_t>& oparam, int32_t size, int32_t index, int32_t acc) {
	if (acc == 0 || index == size)
		return FALSE;
	int32_t o1 = oparam[index] & 0xffff;
	int32_t o2 = oparam[index] >> 16;
	if (index == size - 1)
		return acc == o1 || acc == o2;
	return (acc > o1 && select_sum_check1(oparam, size, index + 1, acc - o1))
		|| (o2 > 0 && acc > o2 && select_sum_check1(oparam, size, index + 1, acc - o2));
}
bool field::process(Processors::SelectSum& arg) {
	auto playerid = arg.playerid;
	auto acc = arg.acc;
	auto min = arg.min;
	auto max = arg.max;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !(core.select_cards.empty())) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "select_with_sum_limit : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%d", min, max);
		fprintf(fp, ",%d", core.select_cards.size());
		for (auto& pcard : core.select_cards) {
			fprintf(fp, ",%d,%d", pcard->fieldid_r, pcard->sum_param);
		}
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li >= 1) {
			return_cards.clear();
			returns.clear();
			for (auto& pcard : core.select_cards) {
				for (auto currval : currvals) {
					if (pcard->fieldid_r == currval) {
						return_cards.list.push_back(pcard);
					}
				}
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_with_sum_limit : ");
				for (auto& pcard : return_cards.list) {
					if (first) {
						first = false;
						fprintf(fp, "%d", pcard->fieldid_r);
					}
					else
						fprintf(fp, ",%d", pcard->fieldid_r);
				}
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					char curr[25];
					char currstr[400];
					sscanf(line, "%s : %s", curr, currstr);
					std::vector<uint32_t> currvals;
					currvals.push_back(0);
					for (int s = 0; s < strlen(currstr); s++) {
						char nowc = currstr[s];
						if (nowc == 0x2c) {
							currvals.push_back(0);
						}
						if ((nowc >= 0x30) && (nowc <= 0x39)) {
							currvals.back() = currvals.back() * 10 + (nowc - 0x30);
						}
					}
					if (!strcmp(curr, "select_with_sum_limit")) {
						return_cards.clear();
						returns.clear();
						for (auto& pcard : pduel->cards) {
							for (auto currval : currvals) {
								if (pcard->fieldid_r == currval) {
									return_cards.list.push_back(pcard);
								}
							}
						}
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				char curr[25];
				char currstr[400];
				sscanf(line, "%s : %s", curr, currstr);
				std::vector<uint32_t> currvals;
				currvals.push_back(0);
				for (int s = 0; s < strlen(currstr); s++) {
					char nowc = currstr[s];
					if (nowc == 0x2c) {
						currvals.push_back(0);
					}
					if ((nowc >= 0x30) && (nowc <= 0x39)) {
						currvals.back() = currvals.back() * 10 + (nowc - 0x30);
					}
				}
				if (!strcmp(curr, "select_with_sum_limit")) {
					return_cards.clear();
					returns.clear();
					for (auto& pcard : pduel->cards) {
						for (auto currval : currvals) {
							if (pcard->fieldid_r == currval) {
								return_cards.list.push_back(pcard);
							}
						}
					}
					fclose(fp);
					pduel->playerop_line++;
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		return_cards.clear();
		returns.clear();
		if (core.select_cards.empty()) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_SELECT_SUM);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		if (max)
			message->write<uint8_t>(0);
		else
			message->write<uint8_t>(1);
		if (max < min)
			max = min;
		message->write<uint32_t>(acc & 0xffff);
		message->write<uint32_t>(min);
		message->write<uint32_t>(max);
		message->write<uint32_t>(core.must_select_cards.size());
		for (auto& pcard : core.must_select_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write(pcard->get_info_location());
			message->write<uint32_t>(pcard->sum_param);
		}
		message->write<uint32_t>(core.select_cards.size());
		std::sort(core.select_cards.begin(), core.select_cards.end(), card::card_operation_sort);
		for (auto& pcard : core.select_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write(pcard->get_info_location());
			message->write<uint32_t>(pcard->sum_param);
		}
		return FALSE;
	}
	else {
		if (!parse_response_cards(false)) {
			return_cards.clear();
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		int32_t tot = static_cast<int32_t>(return_cards.list.size());
		if (max) {
			if (tot < min || tot > max) {
				return_cards.clear();
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			int32_t mcount = static_cast<int32_t>(core.must_select_cards.size());
			std::vector<int32_t> oparam;
			for (auto& list : { &core.must_select_cards , &return_cards.list })
				for (auto& pcard : *list)
					oparam.push_back(pcard->sum_param);
			if (!select_sum_check1(oparam, tot + mcount, 0, acc)) {
				return_cards.clear();
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_with_sum_limit : ");
				for (auto& pcard : return_cards.list) {
					if (first) {
						first = false;
						fprintf(fp, "%d", pcard->fieldid_r);
					}
					else
						fprintf(fp, ",%d", pcard->fieldid_r);
				}
				fprintf(fp, "\n");

				fclose(fp);
			}
			return TRUE;
		}
		else {
			// UNUSED VARIABLE
			// int32_t mcount = core.must_select_cards.size();
			int32_t sum = 0, mx = 0, mn = 0x7fffffff;
			for (auto& list : { &core.must_select_cards , &return_cards.list }) {
				for (auto& pcard : *list) {
					int32_t op = pcard->sum_param;
					int32_t o1 = op & 0xffff;
					int32_t o2 = op >> 16;
					int32_t ms = (o2 && o2 < o1) ? o2 : o1;
					sum += ms;
					mx += (o2 > o1) ? o2 : o1;
					if (ms < mn)
						mn = ms;
				}
			}
			if (mx < acc || sum - mn >= acc) {
				return_cards.clear();
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "select_with_sum_limit : ");
				for (auto& pcard : return_cards.list) {
					if (first) {
						first = false;
						fprintf(fp, "%d", pcard->fieldid_r);
					}
					else
						fprintf(fp, ",%d", pcard->fieldid_r);
				}
				fprintf(fp, "\n");

				fclose(fp);
			}
			return TRUE;
		}
	}
	return TRUE;
}
bool field::process(Processors::SortCard& arg) {
	auto playerid = arg.playerid;
	auto is_chain = arg.is_chain;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) && !((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) && !(core.select_cards.empty())) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "sort_card : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d", core.select_cards.size());
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			returns.clear();
			for (auto& pcard : core.select_cards) {
				for (auto currval : currvals) {
					if (pcard->fieldid_r == currval) {
						returns.data.push_back(currval);
					}
				}
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				bool first = true;
				fprintf(fp, "sort_card : ");
				for (auto& currval : currvals) {
					if (first) {
						first = false;
						fprintf(fp, "%d", currval);
					}
					else
						fprintf(fp, ",%d", currval);
				}
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					char curr[25];
					char currstr[400];
					sscanf(line, "%s : %s", curr, currstr);
					std::vector<uint32_t> currvals;
					currvals.push_back(0);
					for (int s = 0; s < strlen(currstr); s++) {
						char nowc = currstr[s];
						if (nowc == 0x2d) {
							pduel->playerop_line++;
							fclose(fp);
							return TRUE;
						}
						if (nowc == 0x2c) {
							currvals.push_back(0);
						}
						if ((nowc >= 0x30) && (nowc <= 0x39)) {
							currvals.back() = currvals.back() * 10 + (nowc - 0x30);
						}
					}
					if (!strcmp(curr, "sort_card")) {
						returns.clear();
						for (auto currval : currvals) {
							returns.data.push_back(currval);
						}
						fclose(fp);
						pduel->playerop_line++;
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				char curr[25];
				char currstr[400];
				sscanf(line, "%s : %s", curr, currstr);
				std::vector<uint32_t> currvals;
				currvals.push_back(0);
				for (int s = 0; s < strlen(currstr); s++) {
					char nowc = currstr[s];
					if (nowc == 0x2d) {
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
					if (nowc == 0x2c) {
						currvals.push_back(0);
					}
					if ((nowc >= 0x30) && (nowc <= 0x39)) {
						currvals.back() = currvals.back() * 10 + (nowc - 0x30);
					}
				}
				if (!strcmp(curr, "sort_card")) {
					returns.clear();
					for (auto currval : currvals) {
						returns.data.push_back(currval);
					}
					fclose(fp);
					pduel->playerop_line++;
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		returns.clear();
		if ((playerid == 1) && is_flag(DUEL_SIMPLE_AI)) {
			returns.set<int8_t>(0, -1);
			return TRUE;
		}
		if (core.select_cards.empty()) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		auto message = pduel->new_message((is_chain) ? MSG_SORT_CHAIN : MSG_SORT_CARD);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint32_t>(core.select_cards.size());
		for (auto& pcard : core.select_cards) {
			message->write<uint32_t>(pcard->data.code);
			message->write<uint8_t>(pcard->current.controler);
			message->write<uint32_t>(pcard->current.location);
			message->write<uint32_t>(pcard->current.sequence);
		}
		return FALSE;
	}
	else {
		if (returns.at<int8_t>(0) == -1) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "sort_card : ");
			fprintf(fp, "%d", returns.at<int8_t>(0));
			fprintf(fp, "\n");

			fclose(fp);
			return TRUE;
		}
		auto m = static_cast<uint8_t>(core.select_cards.size());
		std::vector<bool> c(m);
		for(uint8_t i = 0; i < m; ++i) {
			auto v = returns.at<int8_t>(i);
			if(v < 0 || v >= m || c[v]) {
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			c[v] = true;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			bool first = true;
			fprintf(fp, "sort_card : ");
			for (uint8_t i = 0; i < m; ++i) {
				if (first) {
					first = false;
					fprintf(fp, "%d", returns.at<int8_t>(i));
				}
				else
					fprintf(fp, ",%d", returns.at<int8_t>(i));
			}
			fprintf(fp, "\n");

			fclose(fp);
		}
		return TRUE;
	}
	return TRUE;
}
bool field::process(Processors::AnnounceRace& arg) {
	auto playerid = arg.playerid;
	auto count = arg.count;
	auto available = arg.available;	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if (arg.step == 0) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "announce_race : ");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "%d,%lld", count, available);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.data.push_back(currvalue);
			returns.data.push_back(currvalue >> 8);
			returns.data.push_back(currvalue >> 16);
			returns.data.push_back(currvalue >> 24);
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_RACE);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "announce_race : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "announce_race")) {
						returns.clear();
						returns.data.push_back(currvalue);
						returns.data.push_back(currvalue >> 8);
						returns.data.push_back(currvalue >> 16);
						returns.data.push_back(currvalue >> 24);
						pduel->playerop_line++;
						fclose(fp);
						auto message = pduel->new_message(MSG_HINT);
						message->write<uint8_t>(HINT_RACE);
						message->write<uint8_t>(playerid);
						message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "announce_race")) {
					returns.clear();
					returns.data.push_back(currvalue);
					returns.data.push_back(currvalue >> 8);
					returns.data.push_back(currvalue >> 16);
					returns.data.push_back(currvalue >> 24);
					pduel->playerop_line++;
					fclose(fp);
					auto message = pduel->new_message(MSG_HINT);
					message->write<uint8_t>(HINT_RACE);
					message->write<uint8_t>(playerid);
					message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if (arg.step == 0) {
		if(count == 0) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_ANNOUNCE_RACE);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(count);
		message->write<uint32_t>(available);
		return FALSE;
	}
	else {
		uint64_t rc = returns.at<uint64_t>(0);
		uint8_t sel = 0;
		for (int32_t ft = 0x1; ft != 0x2000000; ft <<= 1) {
			if (!(ft & rc)) continue;
			if (!(ft & available)) {
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			++sel;
		}
		if (sel != static_cast<uint8_t>(count)) {
		}
		else {
			uint64_t selected_races = returns.at<uint64_t>(0);
			if (bit::has_invalid_bits(selected_races, available)) {
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			if (bit::popcnt(selected_races) != count) {
				pduel->new_message(MSG_RETRY);
				return FALSE;
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "announce_race : %d", returns.at<int32_t>(0));
				fprintf(fp, "\n");
				fclose(fp);
			}
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_RACE);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(returns.at<uint64_t>(0));
			return TRUE;
		}
	}
	return TRUE;
}
bool field::process(Processors::AnnounceAttribute& arg) {
	auto playerid = arg.playerid;
	auto count = arg.count;
	auto available = arg.available;	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if (arg.step == 0) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		fprintf(fp, "%d,", playerid);
		fprintf(fp, "announce_attribute : ");
		fprintf(fp, "%d,%d", count, available);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.data.push_back(currvalue);
			returns.data.push_back(currvalue >> 8);
			returns.data.push_back(currvalue >> 16);
			returns.data.push_back(currvalue >> 24);
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_ATTRIB);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "announce_attribute : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "announce_attribute")) {
						returns.clear();
						returns.data.push_back(currvalue);
						returns.data.push_back(currvalue >> 8);
						returns.data.push_back(currvalue >> 16);
						returns.data.push_back(currvalue >> 24);
						pduel->playerop_line++;
						fclose(fp);
						auto message = pduel->new_message(MSG_HINT);
						message->write<uint8_t>(HINT_ATTRIB);
						message->write<uint8_t>(playerid);
						message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "announce_attribute")) {
					returns.clear();
					returns.data.push_back(currvalue);
					returns.data.push_back(currvalue >> 8);
					returns.data.push_back(currvalue >> 16);
					returns.data.push_back(currvalue >> 24);
					pduel->playerop_line++;
					fclose(fp);
					auto message = pduel->new_message(MSG_HINT);
					message->write<uint8_t>(HINT_ATTRIB);
					message->write<uint8_t>(playerid);
					message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if(arg.step == 0) {
		if(count == 0) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_SELECTMSG);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(0);
			return TRUE;
		}
		auto message = pduel->new_message(MSG_ANNOUNCE_ATTRIB);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(count);
		message->write<uint32_t>(available);
		return FALSE;
	} else {
		auto selected_attributes = returns.at<uint32_t>(0);
		if(bit::has_invalid_bits(selected_attributes, available)) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if(bit::popcnt(selected_attributes) != count) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "announce_attribute : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");

			fclose(fp);
		}
		auto message = pduel->new_message(MSG_HINT);
		message->write<uint8_t>(HINT_ATTRIB);
		message->write<uint8_t>(playerid);
		message->write<uint64_t>(returns.at<uint32_t>(0));
		return TRUE;
	}
	return TRUE;
}
#define BINARY_OP(opcode,op) case opcode: {\
								if (stack.size() >= 2) {\
									auto rhs = stack.top();\
									stack.pop();\
									auto lhs = stack.top();\
									stack.pop();\
									stack.push(lhs op rhs);\
								}\
								break;\
							}
#define UNARY_OP(opcode,op) case opcode: {\
								if (stack.size() >= 1) {\
									auto val = stack.top();\
									stack.pop();\
									stack.push(op val);\
								}\
								break;\
							}
#define UNARY_OP_OP(opcode,val,op) UNARY_OP(opcode,cd.val op)
#define GET_OP(opcode,val) case opcode: {\
								stack.push(cd.val);\
								break;\
							}
static int32_t is_declarable(const card_data& cd, const std::vector<uint64_t>& opcodes) {
	std::stack<int64_t> stack;
	bool alias = false, token = false;
	for (auto& opcode : opcodes) {
		switch (opcode) {
			BINARY_OP(OPCODE_ADD, +);
			BINARY_OP(OPCODE_SUB, -);
			BINARY_OP(OPCODE_MUL, *);
			BINARY_OP(OPCODE_DIV, / );
			BINARY_OP(OPCODE_AND, &&);
			BINARY_OP(OPCODE_OR, || );
			UNARY_OP(OPCODE_NEG, -);
			UNARY_OP(OPCODE_NOT, !);
			BINARY_OP(OPCODE_BAND, &);
			BINARY_OP(OPCODE_BOR, | );
			BINARY_OP(OPCODE_BXOR, ^);
			UNARY_OP(OPCODE_BNOT, ~);
			BINARY_OP(OPCODE_LSHIFT, << );
			BINARY_OP(OPCODE_RSHIFT, >> );
			UNARY_OP_OP(OPCODE_ISCODE, code, == (uint32_t));
			UNARY_OP_OP(OPCODE_ISTYPE, type, &);
			UNARY_OP_OP(OPCODE_ISRACE, race, &);
			UNARY_OP_OP(OPCODE_ISATTRIBUTE, attribute, &);
			GET_OP(OPCODE_GETCODE, code);
			GET_OP(OPCODE_GETTYPE, type);
			GET_OP(OPCODE_GETRACE, race);
			GET_OP(OPCODE_GETATTRIBUTE, attribute);
			//GET_OP(OPCODE_GETSETCARD, setcode);
		case OPCODE_ISSETCARD: {
			if (stack.size() >= 1) {
				int32_t set_code = (int32_t)stack.top();
				stack.pop();
				bool res = false;
				uint16_t settype = set_code & 0xfff;
				uint16_t setsubtype = set_code & 0xf000;
				for (auto& sc : cd.setcodes) {
					if ((sc & 0xfff) == settype && (sc & 0xf000 & setsubtype) == setsubtype) {
						res = true;
						break;
					}
				}
				stack.push(res);
			}
			break;
		}
		case OPCODE_ALLOW_ALIASES: {
			alias = true;
			break;
		}
		case OPCODE_ALLOW_TOKENS: {
			token = true;
			break;
		}
		default: {
			stack.push(opcode);
			break;
		}
		}
	}
	if (stack.size() != 1 || stack.top() == 0)
		return FALSE;
	return cd.code == CARD_MARINE_DOLPHIN || cd.code == CARD_TWINKLE_MOSS
		|| ((alias || !cd.alias) && (token || ((cd.type & (TYPE_MONSTER + TYPE_TOKEN)) != (TYPE_MONSTER + TYPE_TOKEN))));
}
#undef BINARY_OP
#undef UNARY_OP
#undef UNARY_OP_OP
#undef GET_OP
bool field::process(Processors::AnnounceCard& arg) {
	auto playerid = arg.playerid;	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if (arg.step == 0) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		bool first = true;
		fprintf(fp, "announce_card : ");
		fprintf(fp, "%d,", playerid);
		int cs = 0;
		for (auto& option : core.select_options) {
			cs++;
		}
		fprintf(fp, "%d,", cs);
		for (auto& option : core.select_options) {
			if (first) {
				fprintf(fp, "%lld", option);
				first = false;
			}
			else
				fprintf(fp, ",%lld", option);
		}
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.data.push_back(currvalue);
			returns.data.push_back(currvalue >> 8);
			returns.data.push_back(currvalue >> 16);
			returns.data.push_back(currvalue >> 24);
			if (!core.skip_announce) {
				auto message = pduel->new_message(MSG_HINT);
				message->write<uint8_t>(HINT_CODE);
				message->write<uint8_t>(playerid);
				message->write<uint64_t>(currvalue);
			}
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "announce_card : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "announce_card")) {
						returns.clear();
						returns.data.push_back(currvalue);
						returns.data.push_back(currvalue >> 8);
						returns.data.push_back(currvalue >> 16);
						returns.data.push_back(currvalue >> 24);
						pduel->playerop_line++;
						fclose(fp);
						if (!core.skip_announce) {
							auto message = pduel->new_message(MSG_HINT);
							message->write<uint8_t>(HINT_CODE);
							message->write<uint8_t>(playerid);
							message->write<uint64_t>(currvalue);
						}
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "announce_card")) {
					returns.clear();
					returns.data.push_back(currvalue);
					returns.data.push_back(currvalue >> 8);
					returns.data.push_back(currvalue >> 16);
					returns.data.push_back(currvalue >> 24);
					pduel->playerop_line++;
					fclose(fp);
					if (!core.skip_announce) {
						auto message = pduel->new_message(MSG_HINT);
						message->write<uint8_t>(HINT_CODE);
						message->write<uint8_t>(playerid);
						message->write<uint64_t>(currvalue);
					}
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if (arg.step == 0) {
		auto message = pduel->new_message(MSG_ANNOUNCE_CARD);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(static_cast<uint8_t>(core.select_options.size()));
		for (auto& option : core.select_options)
			message->write<uint64_t>(option);
		return FALSE;
	}
	else {
		int32_t code = returns.at<int32_t>(0);
		const auto& data = pduel->read_card(code);
		if (!data.code || !is_declarable(data, core.select_options)) {
			/*auto message = */pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "announce_card : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");

			fclose(fp);
		}
		if (!core.skip_announce) {
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_CODE);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(code);
		}
		return TRUE;
	}
	return TRUE;
}
bool field::process(Processors::AnnounceNumber& arg) {
	auto playerid = arg.playerid;
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if (arg.step == 0) {
		char fc[50];
		if (plconf) sprintf_s(fc, "./playeroplast.log"); else sprintf_s(fc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "a+");
		bool first = true;
		fprintf(fp, "announce_number : ");
		fprintf(fp, "%d,", playerid);
		int cs = 0;
		for (auto& option : core.select_options) {
			cs++;
		}
		fprintf(fp, "%d,", cs);
		for (auto& option : core.select_options) {
			if (first) {
				fprintf(fp, "%lld", option);
				first = false;
			}
			else
				fprintf(fp, ",%lld", option);
		}
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, playerid);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 1) {
			int currvalue = currvals[0];
			returns.clear();
			returns.data.push_back(currvalue);
			returns.data.push_back(currvalue >> 8);
			returns.data.push_back(currvalue >> 16);
			returns.data.push_back(currvalue >> 24);
			auto message = pduel->new_message(MSG_HINT);
			message->write<uint8_t>(HINT_NUMBER);
			message->write<uint8_t>(playerid);
			message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "announce_number : %d", currvalue);
				fprintf(fp, "\n");
				fclose(fp);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currvalue;
					char curr[25];
					sscanf(line, "%s : %d", curr, &currvalue);
					if (!strcmp(curr, "announce_number")) {
						returns.clear();
						returns.data.push_back(currvalue);
						returns.data.push_back(currvalue >> 8);
						returns.data.push_back(currvalue >> 16);
						returns.data.push_back(currvalue >> 24);
						pduel->playerop_line++;
						fclose(fp);
						auto message = pduel->new_message(MSG_HINT);
						message->write<uint8_t>(HINT_NUMBER);
						message->write<uint8_t>(playerid);
						message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currvalue;
				char curr[25];
				sscanf(line, "%s : %d", curr, &currvalue);
				if (!strcmp(curr, "announce_number")) {
					returns.clear();
					returns.data.push_back(currvalue);
					returns.data.push_back(currvalue >> 8);
					returns.data.push_back(currvalue >> 16);
					returns.data.push_back(currvalue >> 24);
					pduel->playerop_line++;
					fclose(fp);
					auto message = pduel->new_message(MSG_HINT);
					message->write<uint8_t>(HINT_NUMBER);
					message->write<uint8_t>(playerid);
					message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	if (arg.step == 0) {
		auto message = pduel->new_message(MSG_ANNOUNCE_NUMBER);
		effect_set eset;
		filter_player_effect(playerid, EFFECT_PROMISED_END, &eset);
		if (eset.size())
			message->write<uint8_t>(1 - playerid);
		else
			message->write<uint8_t>(playerid);
		message->write<uint8_t>(static_cast<uint8_t>(core.select_options.size()));
		for (auto& option : core.select_options)
			message->write<uint64_t>(option);
		return FALSE;
	}
	else {
		int32_t ret = returns.at<int32_t>(0);
		if (ret < 0 || ret >= (int32_t)core.select_options.size()) {
			pduel->new_message(MSG_RETRY);
			return FALSE;
		}
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "announce_number : %d", returns.at<int32_t>(0));
			fprintf(fp, "\n");
			fclose(fp);
		}
		auto message = pduel->new_message(MSG_HINT);
		message->write<uint8_t>(HINT_NUMBER);
		message->write<uint8_t>(playerid);
		message->write<uint64_t>(core.select_options[returns.at<int32_t>(0)]);
		return TRUE;
	}
}
bool field::process(Processors::RockPaperScissors& arg) {
	char pfc[50];
	sprintf_s(pfc, "./playerop.conf");
	FILE *pfp = NULL;
	fopen_s(&pfp, pfc, "r");
	int plconf = 0;
	if (pfp) {
		char conf[40];
		fgets(conf, 40, pfp);
		char plop[10];
		sscanf(conf, "%s = %d", plop, &plconf);
		fclose(pfp);
	}
	if ((arg.step == 0) || (arg.step == 1)) {
		char ffc[50];
		if (plconf) sprintf_s(ffc, "./playeroplast.log"); else sprintf_s(ffc, "./playeroplast %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, ffc, "a+");
		fprintf(fp, "rock_paper_scissors : ");
		fprintf(fp, "%d,", arg.step);
		fprintf(fp, "%d", 3);
		fprintf(fp, "\n");
		fclose(fp);
		int li = 0;
		std::vector<uint32_t> currvals;
		luaL_checkstack(pduel->lua->lua_state, 5, nullptr);
		lua_getglobal(pduel->lua->lua_state, "playerop_analyze");
		if (!lua_isnil(pduel->lua->lua_state, -1) && !pduel->playerop_config) {
			lua_pushinteger(pduel->lua->lua_state, arg.step);
			lua_pushinteger(pduel->lua->lua_state, pduel->playerop_seed[0]);
			lua_pcall(pduel->lua->lua_state, 2, 0, 0);
			bool ln = true;
			while (ln) {
				lua_getglobal(pduel->lua->lua_state, "playerop_table");
				if (lua_istable(pduel->lua->lua_state, -1)) {
					if (!li)
						li++;
					lua_pushinteger(pduel->lua->lua_state, li);
					lua_gettable(pduel->lua->lua_state, -2);
					if (!lua_isnil(pduel->lua->lua_state, -1)) {
						int lj = lua_tointeger(pduel->lua->lua_state, -1);
						lua_getglobal(pduel->lua->lua_state, "playerop_debug");
						if (!lua_isnil(pduel->lua->lua_state, -1)) {
							lua_pushinteger(pduel->lua->lua_state, lj);
							lua_call(pduel->lua->lua_state, 1, 0);
						}
						currvals.push_back(lj);
						lua_settop(pduel->lua->lua_state, 0);
						li++;
					}
					else
						ln = false;
					lua_settop(pduel->lua->lua_state, 0);
				}
				else {
					
					ln = false;
				}
				lua_settop(pduel->lua->lua_state, 0);
			}
		}
		lua_settop(pduel->lua->lua_state, 0);
		if (li > 2) {
			int currval1 = currvals[0];
			int currval2 = currvals[1];
			int32_t hand0 = currval1;
			int32_t hand1 = currval2;
			if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
				char fc[50];
				if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
				FILE *fp = NULL;
				fopen_s(&fp, fc, "a+");
				fprintf(fp, "rock_paper_scissors : %d,%d", hand0, hand1);
				fprintf(fp, "\n");
				fclose(fp);
			}
			auto message = pduel->new_message(MSG_HAND_RES);
			message->write<uint8_t>(hand0 + (hand1 << 2));
			if (hand0 == hand1) {
				if (arg.repeat) {
					pduel->playerop_line++;
					fclose(fp);
					return FALSE;
				}
				else
					returns.set<int32_t>(0, PLAYER_NONE);
			}
			else if ((hand0 == 1 && hand1 == 2) || (hand0 == 2 && hand1 == 3) || (hand0 == 3 && hand1 == 1)) {
				returns.set<int32_t>(0, 1);
			}
			else {
				returns.set<int32_t>(0, 0);
			}
			return TRUE;
		}
	}
	if ((plconf == 1) || (/*!plconf &&*/ pduel->playerop_config)) {
		char vfc[50];
		if (plconf) sprintf_s(vfc, "playeropvirtual.log"); else sprintf_s(vfc, "playeropvirtual %lld.log", pduel->playerop_seed[0]);
		const char *zfc = vfc;
		if (pduel->qlayerop_line && (pduel->playerop_line > pduel->qlayerop_line) && (access(zfc, 0) != -1)) {
			int line_count = 0;
			char fc[50];
			if (plconf) sprintf_s(fc, "./playeropvirtual.log"); else sprintf_s(fc, "./playeropvirtual %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "r");
			char line[400];
			while (fgets(line, 400, fp) != NULL) {
				line_count++;
				if (line_count == pduel->playerop_line - pduel->qlayerop_line) {
					int currval1, currval2;
					char curr[25];
					sscanf(line, "%s : %d,%d", curr, &currval1, &currval2);
					if (!strcmp(curr, "rock_paper_scissors")) {
						int32_t hand0 = currval1;
						int32_t hand1 = currval2;
						auto message = pduel->new_message(MSG_HAND_RES);
						message->write<uint8_t>(hand0 + (hand1 << 2));
						if (hand0 == hand1) {
							if (arg.repeat) {
								pduel->playerop_line++;
								fclose(fp);
								return FALSE;
							}
							else
								returns.set<int32_t>(0, PLAYER_NONE);
						}
						else if ((hand0 == 1 && hand1 == 2) || (hand0 == 2 && hand1 == 3) || (hand0 == 3 && hand1 == 1)) {
							returns.set<int32_t>(0, 1);
						}
						else {
							returns.set<int32_t>(0, 0);
						}
						pduel->playerop_line++;
						fclose(fp);
						return TRUE;
					}
				}
				if (line_count > pduel->playerop_line - pduel->qlayerop_line)
					break;
			}
			fclose(fp);
		}
		int line_count = 0;
		char fc[50];
		if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
		FILE *fp = NULL;
		fopen_s(&fp, fc, "r");
		char line[400];
		while (fgets(line, 400, fp) != NULL) {
			line_count++;
			if (line_count == pduel->playerop_line) {
				int currval1, currval2;
				char curr[25];
				sscanf(line, "%s : %d,%d", curr, &currval1, &currval2);
				if (!strcmp(curr, "rock_paper_scissors")) {
					int32_t hand0 = currval1;
					int32_t hand1 = currval2;
					auto message = pduel->new_message(MSG_HAND_RES);
					message->write<uint8_t>(hand0 + (hand1 << 2));
					if (hand0 == hand1) {
						if (arg.repeat) {
							pduel->playerop_line++;
							fclose(fp);
							return FALSE;
						}
						else
							returns.set<int32_t>(0, PLAYER_NONE);
					}
					else if ((hand0 == 1 && hand1 == 2) || (hand0 == 2 && hand1 == 3) || (hand0 == 3 && hand1 == 1)) {
						returns.set<int32_t>(0, 1);
					}
					else {
						returns.set<int32_t>(0, 0);
					}
					pduel->playerop_line++;
					fclose(fp);
					return TRUE;
				}
			}
			if (line_count > pduel->playerop_line)
				break;
		}
		fclose(fp);
	}
	auto checkvalid = [&] {
		const auto ret = returns.at<int32_t>(0);
		if(ret < 1 || ret > 3) {
			pduel->new_message(MSG_RETRY);
			--arg.step;
			return false;
		}
		return true;
	};
	auto repeat = arg.repeat;
	switch(arg.step) {
	case 0: {
		auto message = pduel->new_message(MSG_ROCK_PAPER_SCISSORS);
		message->write<uint8_t>(0);
		return FALSE;
	}
	case 1: {
		if(checkvalid()) {
			arg.hand0 = returns.at<int32_t>(0);
			auto message = pduel->new_message(MSG_ROCK_PAPER_SCISSORS);
			message->write<uint8_t>(1);
		}
		return FALSE;
	}
	case 2: {
		if (!checkvalid())
			return FALSE;
		auto hand0 = arg.hand0;
		auto hand1 = static_cast<uint8_t>(returns.at<int32_t>(0));
		if ((plconf == 2) || (!plconf && !pduel->playerop_config)) {
			char fc[50];
			if (plconf) sprintf_s(fc, "./playerop.log"); else sprintf_s(fc, "./playerop %lld.log", pduel->playerop_seed[0]);
			FILE *fp = NULL;
			fopen_s(&fp, fc, "a+");
			fprintf(fp, "rock_paper_scissors : %d,%d", hand0, hand1);
			fprintf(fp, "\n");
			fclose(fp);
		}
		auto message = pduel->new_message(MSG_HAND_RES);
		message->write<uint8_t>(hand0 + (hand1 << 2));
		if (hand0 == hand1) {
			if (repeat) {
				message = pduel->new_message(MSG_ROCK_PAPER_SCISSORS);
				message->write<uint8_t>(0);
				arg.step = 0;
				return FALSE;
			}
			else
				returns.set<int32_t>(0, PLAYER_NONE);
		}
		else if ((hand0 == 1 && hand1 == 2) || (hand0 == 2 && hand1 == 3) || (hand0 == 3 && hand1 == 1)) {
			returns.set<int32_t>(0, 1);
		}
		else {
			returns.set<int32_t>(0, 0);
		}
		return TRUE;
	}
	}
	return TRUE;
}
