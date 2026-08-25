#ifndef ATG_ENGINE_SIM_ENGINE_CATALOG_H
#define ATG_ENGINE_SIM_ENGINE_CATALOG_H

#include <string>
#include <vector>

struct EngineCatalogEntry {
    std::string group;
    std::string name;
    std::string relativeScriptPath;
};

/*
 * Rebuild the catalog.
 *
 * On iOS this also rescans:
 *
 * Documents/Custom Engines
 */
void refreshEngineCatalog();

/*
 * Return the currently cached catalog.
 *
 * This function NEVER rebuilds the vector, so references remain
 * stable until refreshEngineCatalog() is explicitly called.
 */
const std::vector<EngineCatalogEntry> &engineCatalog();

#endif
