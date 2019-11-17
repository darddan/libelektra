/**
 * @file
 *
 * @brief Delegate implementation for the `kconfig` plugin
 *
 * @copyright BSD License (see LICENSE.md or https://www.libelektra.org)
 *
 */

#include "kconfig_delegate.hpp"
#include "file_utility.hpp"
#include "kconfig_parser.hpp"

using kdb::Key;
using kdb::KeySet;

namespace elektra
{

/**
 * @brief This constructor creates a new delegate object used by the `kconfig` plugin
 *
 * @param config This key set contains configuration values provided by the `kconfig` plugin
 */
KconfigDelegate::KconfigDelegate (KeySet config)
{
	configuration = config;
}

/**
 * @brief This method returns the configuration of the plugin, prefixing key names with the name of `parent`.
 *
 * @param parent This key specifies the name this function adds to the stored configuration values.
 *
 * @return A key set storing the configuration values of the plugin
 */
kdb::KeySet KconfigDelegate::getConfig (Key const & parent)
{
	KeySet keys{ 0, KS_END };

	try
	{
		ELEKTRA_LOG_DEBUG ("Parse `%s` using the kconfig plugin", parent.getString ().c_str ());
		FileUtility fileUtility{ parent.getString () };

		ELEKTRA_LOG_DEBUG ("The file opened successfully. Start parsing");
		KConfigParser parser{ fileUtility, keys };
		parser.parse (parent);

		ELEKTRA_LOG_DEBUG ("Parsing finished successfully");
	}
	catch (KConfigParserException & e)
	{
		// TODO: Handle the error properly
		throw std::runtime_error (e.getMessage ());
	}

	return keys;
}

} // end namespace elektra