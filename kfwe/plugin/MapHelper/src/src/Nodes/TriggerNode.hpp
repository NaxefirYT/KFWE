#pragma once

#include "Node.h"
#include "Function.hpp"
#include <set>
#include <base/util/colored_cout.h>

namespace mh {
	//trigger node
	class TriggerNode : public Node {
	public:
		static NodePtr From(void* trigger, uint32_t childId, NodePtr parent) {
			return NodePtr(new TriggerNode((Trigger*)trigger));
		}

		virtual void* getData() override { return m_trigger; }

		virtual const std::string& getName() override { return m_name; }

		virtual uint32_t getNameId() override { return m_nameId; }

		virtual TYPE getType() override { return TYPE::ROOT; }

		virtual void setType(TYPE type) override { }

		virtual NodePtr getParentNode() override { return nullptr; }

		virtual NodePtr getRootNode() override { return shared_from_this(); }

		virtual std::vector<NodePtr> getChildList() override {
			std::vector<NodePtr> list;

			NodePtr node = shared_from_this();

			if (m_trigger->line_count > 0 && m_trigger->actions) {
				for (uint32_t i = 0; i < m_trigger->line_count; i++) {
					Action* action = m_trigger->actions[i];
					if (!action->enable || action->group_id != -1) //Skip when disabled or when an action is not part of the root node.
						continue;
					NodePtr child = NodeFromAction(action, i, node);
					list.push_back(child);
				}
			}
			return list;
		}

		virtual bool getValue(const NodeFilter& filter) override {
			return true;
		}

		virtual std::string toString(TriggerFunction* func) override {
		
	
			TriggerFunction tfunc(m_funcName, m_name);
			
			func = &tfunc;


			std::vector<std::vector<NodePtr>> nodes(4);
			
			for (auto& node : getChildList()) {
				Action* action = (Action*)node->getData();
				auto type = get_action_type(action);
				nodes[type].push_back(node);
			}

			//event
			{
				auto events = func->event;
				func->push(events);

				current_progress = Action::Type::event;

				*events << "\tset " + m_trigger_variable_name + " = CreateTrigger()\n";
				if (m_trigger->is_disable_init) {
					*events << "\tcall DisableTrigger(" + m_trigger_variable_name + ")\n";
				}

				*events << "#ifdef DEBUG\n";
				*events << "\tcall YDWESaveTriggerName(" + m_trigger_variable_name + ",\"" + m_name + "\")\n";
				*events << "#endif\n";

				for (auto& node : nodes[Action::Type::event]) {
					*events << node->toString(func);
				}

				if (!nodes[Action::Type::condition].empty()) {
					*events << "\tcall TriggerAddCondition(" + m_trigger_variable_name + ", Condition(function " + func->condition->getName() + "))\n";
				}
				*events << "\tcall TriggerAddAction(" + m_trigger_variable_name + ", function " + func->action->getName() + ")\n";
				func->pop();
			}
			 
			//condition
			{
				current_progress = Action::Type::condition;

				auto conditions = func->condition;
				func->push(conditions);
				for (auto& node : nodes[Action::Type::condition]) {
					if (!conditions->isEmpty()) {
						*conditions << " and ";
					} else {
						*conditions << "\treturn ";
					}
					node->setType(TYPE::GET);
					*conditions << "(" + node->toString(func) + ")";
				}
				if (!conditions->isEmpty()) {
					*conditions << "\n";
				}
				func->pop();

				if (!conditions->isEmpty()) {
					func->addFunction(conditions);
				}
			}
			
			//action 
			{
				current_progress = Action::Type::action;

				auto actions = func->action;
				func->push(actions);
				for (auto& node : nodes[Action::Type::action]) {
					*actions << node->toString(func);
				} 
				func->pop();
				//If the current trigger contains an unpredictable variable, insert two code snippets at the beginning and end.
				if (hasUpvalue) {
					actions->insert_begin += func->getSpaces() + "YDLocalInitialize()\n";
					actions->insert_end += func->getSpaces() + "call YDLocal1Release()\n";
					actions->insert_end += func->getSpaces() + "call PK_UnitLoopClear()\n";
				}
			} 

			auto& world = get_world_editor();
			auto& editor = get_trigger_editor();
			//Checking for unconventional variable types and issuing a warning.
			if (g_make_editor_data) {
				for (auto&& [name, map] : all_upvalue_map) {
					if (map.size() > 1) {
						std::string warning = std::format("Warning: trigger[{}]: [Unbelievable] local variable[loc_{}] There are multiple types; please fix them as soon as possible.\n", base::u2a(m_name), base::u2a(name));
						print(warning.c_str());
						for (auto&& [type, info] : map) {
							std::string type_name = world.getConfigData("TriggerTypes", type, 3);
							print("[%s] %s\n", base::u2a(type_name).c_str(), base::u2a(info.code).c_str());
						}
						print("\n");
					}
				}
			} else {
				for (auto&& [name, map] : all_upvalue_map) {
					if (map.size() > 1) {
						auto label = "loc_" + name;

						auto warning = std::format("<yellow>Warning: trigger[<red>{}</red>]: [Unbelievable] local variable[<red>loc_{}</red>] There are multiple types; please fix them as soon as possible.</yellow>", base::u2a(m_name), base::u2a(name));
						console_color_output(warning);
						for (auto&& [type, info] : map) {
							auto type_name = world.getConfigData("TriggerTypes", type, 3);
							auto msg = std::format("<yellow>-----------[{}]-----------</yellow>",base::u2a(type_name));
							console_color_output(msg);
							editor.action_to_text_key = name;
							console_color_output(std::format("<while>{}</while>", base::u2a(editor.originConvertActionText(info.action))));
						}
						std::cout << std::endl;
					}
				}
			}
			
			
			

			editor.m_initFuncTable[func->event->getName()] = true;

			return func->toString();
		}


		virtual std::string getFuncName() override {
			return m_funcName;
		}

		virtual const std::string& getTriggerVariableName() override {
			return m_trigger_variable_name;
		}

		virtual std::string getUpvalue(const Upvalue& info) override {
			std::string result;

			//What is passed is a function name.
			if (info.uptype == Upvalue::TYPE::GET_LOCAL && info.is_func) {
				return info.name + "()";
			}

			if (current_progress == Action::Type::action) { //Compiling trigger action
				switch (info.uptype) {
				case Upvalue::TYPE::SET_LOCAL:
					result = std::format("YDLocal1Set({}, \"{}\", {})", info.type, info.name, info.value);
					break;
				case Upvalue::TYPE::GET_LOCAL:
					result = std::format("YDLocal1Get({}, \"{}\")", info.type, info.name);
					break;
				case Upvalue::TYPE::SET_ARRAY:
					result = std::format("YDLocal1ArraySet({}, \"{}\", {}, {})", info.type, info.name, info.index, info.value);
					break;
				case Upvalue::TYPE::GET_ARRAY:
					result = std::format("YDLocal1ArrayGet({}, \"{}\", {})", info.type, info.name, info.index);
					break;
				default:
					break;
				}
			} else { //Otherwise, it's a compilation event and condition.
				switch (info.uptype) {
				case Upvalue::TYPE::SET_LOCAL:
					result = std::format("YDLocal2Set({}, \"{}\", {})", info.type, info.name, info.value);
					break;
				case Upvalue::TYPE::GET_LOCAL:
					result = std::format("YDLocal2Get({}, \"{}\")", info.type, info.name);
					break;
				case Upvalue::TYPE::SET_ARRAY:
					result = std::format("YDLocal2ArraySet({}, \"{}\", {}, {})", info.type, info.name, info.index, info.value);
					break;
				case Upvalue::TYPE::GET_ARRAY:
					result = std::format("YDLocal2ArrayGet({}, \"{}\", {})", info.type, info.name, info.index);
					break;
				default:
					break;
				}
			}
			
			return result;
		}

	public:
		bool hasUpvalue = false;

		// all_upvalue_map[name] = {type, action}
		struct WarningInfo {
			std::string code;
			Action* action = nullptr;
		};
		std::map<std::string, std::map<std::string, WarningInfo>> all_upvalue_map;

		Action::Type current_progress;

	private:
		TriggerNode(Trigger* trigger) {
			m_trigger = trigger;

			std::string name = trigger->name;
			convert_name(name);

			m_trigger_variable_name = "gg_trg_" + name;
			m_funcName = std::string("Trig_" + name);

			m_name = std::string(trigger->name);
			m_nameId = hash_(m_name.c_str());
		}
	protected:
		Trigger* m_trigger;
		std::string m_name;
		uint32_t m_nameId;

		std::string m_funcName;
		std::string m_trigger_variable_name;

	};

}