-- Test-owned campaign content, not the installed Arulco campaign script.
-- The native arrival callback must invoke the real Lua bridge with these args.
function HandleSectorLiberation(x, y, z, firstTime)
    assert((x == 10 or x == 9) and y == 1 and z == 0 and firstTime)
    -- Distinguish the outward journey from a native retreat's return leg.
    AddVolunteers(x == 10 and 7 or 11)
end
