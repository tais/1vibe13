/** 
 * @file
 * @author feynman (bears-pit.com)
 */

#ifndef PLAN_FACTORY_LIBRARY_H_
#define PLAN_FACTORY_LIBRARY_H_

#include <string>
#include <map>
#include <deque>

class TacticalActor;

namespace AI
{
    namespace tactical
    {
        // forward declarations
        class AbstractPlanFactory;
        class Plan;
        class AIInputData;

        /**@class PlanFactoryLibrary
         * @brief Singleton. A "library" containing all available PlanFactories, accessible via the strings returned by
         * their get_name() function.
         *
         * Used as one "entry point" for the new AI system within the legacy code (the other being the 'execute()' methods of Plan)
         * Handles the mapping of a soldier AI-planning component's plan index to a concrete plan factory instantiation.
         */
        class PlanFactoryLibrary
        {
            private:
                /// Only instance, required for singleton pattern implementation
                static PlanFactoryLibrary* instance_;
                /// A mapping of factory names to pointers of said factories
                std::map<std::string, AbstractPlanFactory*> registred_factories_;
                /// A mapping of soldier plan indices to factory pointers
                std::deque<AbstractPlanFactory*> ai_index_to_factory_mapping_;
                PlanFactoryLibrary();
            public:
                static PlanFactoryLibrary* instance();
                Plan* create_plan(size_t index, TacticalActor* npc, const AIInputData& input) const;
                void update_plan(size_t intdex, TacticalActor* npc, const AIInputData& input) const;
        };
    }
}

#endif

