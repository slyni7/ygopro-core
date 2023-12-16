/*
 * duel.cpp
 *
 *  Created on: 2010-5-2
 *      Author: Argon
 */

#include "duel.h"
#include "interpreter.h"
#include "field.h"
#include "card.h"
#include "effect.h"
#include "group.h"

duel::duel(const OCG_DuelOptions& options) :
	random(std::array<uint64_t,4>{ { options.seed[0], options.seed[1], options.seed[2], options.seed[3] } }),
	read_card_callback(options.cardReader), read_script_callback(options.scriptReader),
	handle_message_callback(options.logHandler), read_card_done_callback(options.cardReaderDone),
	read_card_payload(options.payload1), read_script_payload(options.payload2),
	handle_message_payload(options.payload3), read_card_done_payload(options.payload4)
{
	lua = new interpreter(this, options);
	game_field = new field(this, options);
	game_field->temp_card = new_card(0);
}
duel::~duel() {
	for(auto& pcard : cards)
		delete pcard;
	for(auto& pgroup : groups)
		delete pgroup;
	for(auto& peffect : effects)
		delete peffect;
	for (auto& pduel : dummy_duels)
		delete pduel;
	for (auto& pcard : dummy_cards)
		delete pcard;
	for (auto& pgroup : dummy_groups)
		delete pgroup;
	for (auto& peffect : dummy_effects)
		delete peffect;
	delete game_field;
	delete lua;
}
#if defined(__GNUC__) || defined(__clang_analyzer__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
void duel::clear() {
	static constexpr OCG_DuelOptions default_options{ {},0,{8000,5,1},{8000,5,1} };
	for(auto& pcard : cards)
		delete pcard;
	for(auto& pgroup : groups) {
		lua->unregister_group(pgroup);
		delete pgroup;
	}
	for(auto& peffect : effects) {
		lua->unregister_effect(peffect);
		delete peffect;
	}
	delete game_field;
	cards.clear();
	groups.clear();
	effects.clear();
	game_field = new field(this, default_options);
	game_field->temp_card = new_card(0);
}
void duel::dummy() {
	OCG_DuelOptions default_options{ {},0,{8000,5,1},{8000,5,1} };
	cards.clear();
	groups.clear();
	effects.clear();
	lua = new interpreter(this, default_options);
	game_field = new field(this, default_options);
	random = std::array<uint64_t, 4>{ { playerop_seed[0], playerop_seed[1], playerop_seed[2], playerop_seed[3] } };
	game_field->temp_card = new_card(0);
}
void duel::shahrazad() {
	static constexpr OCG_DuelOptions default_options{ {},0,{8000,5,1},{8000,5,1} };
	std::unordered_set<card*> scards;
	for (auto& pcard : cards)
		scards.insert(pcard);
	shahrazad_cards.push_back(scards);
	cards.clear();
	std::unordered_set<group*> sgroups;
	for (auto& pgroup : groups)
		sgroups.insert(pgroup);
	shahrazad_groups.push_back(sgroups);
	groups.clear();
	std::unordered_set<effect*> seffects;
	for (auto& peffect : effects)
		seffects.insert(peffect);
	shahrazad_effects.push_back(seffects);
	effects.clear();
	shahrazad_fields.push_back(game_field);
	game_field = new field(this, default_options);
	game_field->temp_card = new_card(0);
	shahrazad_count++;
}
void duel::shahrazad_out() {
	for (auto& pcard : cards)
		delete pcard;
	for (auto& pgroup : groups) {
		lua->unregister_group(pgroup);
		delete pgroup;
	}
	for (auto& peffect : effects) {
		lua->unregister_effect(peffect);
		delete peffect;
	}
	delete game_field;
	cards.clear();
	groups.clear();
	effects.clear();
	auto scards = shahrazad_cards.back();
	for (auto& pcard : scards)
		cards.insert(pcard);
	auto sgroups = shahrazad_groups.back();
	for (auto& pgroup : sgroups)
		groups.insert(pgroup);
	auto seffects = shahrazad_effects.back();
	for (auto& peffect : seffects)
		effects.insert(peffect);
	game_field = shahrazad_fields.back();
	shahrazad_cards.pop_back();
	shahrazad_groups.pop_back();
	shahrazad_effects.pop_back();
	shahrazad_fields.pop_back();
	shahrazad_count--;
	game_field->reload_field_info();
	game_field->raise_event((card*)0, 18453728, 0, REASON_EFFECT, shahrazad_result, shahrazad_result, 0);
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
card* duel::new_card(uint32_t code) {
	card* pcard = new card(this);
	cards.insert(pcard);
	if(code)
		pcard->data = read_card(code);
	pcard->data.code = code;
	lua->register_card(pcard);
	return pcard;
}
effect* duel::new_effect() {
	effect* peffect = new effect(this);
	effects.insert(peffect);
	lua->register_effect(peffect);
	return peffect;
}
void duel::delete_card(card* pcard) {
	cards.erase(pcard);
	delete pcard;
}
void duel::delete_group(group* pgroup) {
	lua->unregister_group(pgroup);
	groups.erase(pgroup);
	sgroups.erase(pgroup);
	delete pgroup;
}
void duel::delete_effect(effect* peffect) {
	lua->unregister_effect(peffect);
	effects.erase(peffect);
	delete peffect;
}
void duel::generate_buffer() {
	for(auto& message : messages) {
		uint32_t size = static_cast<uint32_t>(message.data.size());
		if(size) {
			write_buffer(&size, sizeof(size));
			write_buffer(message.data.data(), size);
		}
	}
	messages.clear();
}
void duel::release_script_group() {
	for(auto& pgroup : sgroups) {
		if(pgroup->is_readonly == 0) {
			lua->unregister_group(pgroup);
			groups.erase(pgroup);
			delete pgroup;
		}
	}
	sgroups.clear();
}
void duel::restore_assumes() {
	for(auto& pcard : assumes)
		pcard->assume.clear();
	assumes.clear();
}
void duel::write_buffer(const void* data, size_t size) {
	if(size) {
		const auto vec_size = buff.size();
		buff.resize(vec_size + size);
		std::memcpy(&buff[vec_size], data, size);
	}
}
void duel::clear_buffer() {
	buff.clear();
}
void duel::set_response(const void* resp, size_t len) {
	game_field->returns.clear();
	game_field->returns.data.resize(len);
	if(len)
		std::memcpy(game_field->returns.data.data(), resp, len);
}
// uniform integer distribution
int32_t duel::get_next_integer(int32_t l, int32_t h) {
	const int32_t range = h - l + 1;
	const int32_t lim = random.max() % range;
	int32_t n;
	do {
		n = random();
	} while(n <= lim);
	return static_cast<int32_t>((n % range) + l);
}
duel::duel_message* duel::new_message(uint8_t message) {
	if (skipmsg)
		messages.emplace_back(0xff);
	else
		messages.emplace_back(message);
	return &(messages.back());
}
const card_data& duel::read_card(uint32_t code) {
	card_data* ret;
	auto search = data_cache.find(code);
	if(search != data_cache.end()) {
		ret = &(search->second);
	} else {
		OCG_CardData data{};
		read_card_callback(read_card_payload, code, &data);
		ret = &(data_cache.emplace(code, data).first->second);
		read_card_done_callback(read_card_done_payload, &data);
	}
	return *ret;
}
duel::duel_message::duel_message(uint8_t message) {
	write<uint8_t>(message);
}
void duel::duel_message::write(const void* buff, size_t size) {
	if(size) {
		const auto vec_size = data.size();
		data.resize(vec_size + size);
		std::memcpy(&data[vec_size], buff, size);
	}
}
void duel::duel_message::write(loc_info loc) {
	write<uint8_t>(loc.controler);
	write<uint8_t>(loc.location);
	write<uint32_t>(loc.sequence);
	write<uint32_t>(loc.position);
}

card_data::card_data(const OCG_CardData& data) {
#define COPY(val) this->val = data.val;
	COPY(code);
	COPY(alias);
	COPY(type);
	COPY(level);
	COPY(attribute);
	COPY(race);
	COPY(attack);
	COPY(defense);
	COPY(lscale);
	COPY(rscale);
	COPY(link_marker);
	COPY(ot);
#undef COPY
	//this->name = data.name;
	if(data.setcodes == nullptr)
		return;
	uint16_t sc = 0;
	uint16_t* ptr = data.setcodes;
	for(;;) {
		std::memcpy(&sc, ptr++, sizeof(uint16_t));
		if(sc == 0)
			break;
		this->setcodes.insert(sc);
	}
}
