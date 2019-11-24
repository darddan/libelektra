#include "treemodel.hpp"
#include "modules.hpp"
#include <merging/automergeconfiguration.hpp>
#include <merging/threewaymerge.hpp>

TreeModel::TreeModel (kdb::tools::merging::MergingKDB * kdb, QObject * parentModel)
{
	Q_UNUSED (parentModel);
	m_roleNameMapping[NameRole] = "name";

	m_kdb = kdb;
	setupModelData ();
}

TreeModel::~TreeModel ()
{
	delete m_rootItem;
}

QVariant TreeModel::data (const QModelIndex & index, int role) const
{
	TreeItem * item;
	if (!index.isValid ())
	{
		item = m_rootItem;
	}
	else
	{
		item = static_cast<TreeItem *> (index.internalPointer ());
	}

	switch (role)
	{
	case NameRole:
		return item->name ();
	case ChildrenHaveNoChildrenRole:
		return !item->expandable ();
	case PathRole:
		return item->path ();
	default:
		return {};
	}
}

Qt::ItemFlags TreeModel::flags (const QModelIndex & index) const
{
	if (!index.isValid ())
	{
		return Qt::NoItemFlags;
	}

	return QAbstractItemModel::flags (index);
}

QVariant TreeModel::headerData (int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		return m_rootItem->data (section);
	}

	return QVariant ();
}

QModelIndex TreeModel::index (int row, int column, const QModelIndex & parent) const
{
	if (!hasIndex (row, column, parent))
	{
		return {};
	}

	TreeItem * parentItem;

	if (parent.isValid ())
	{
		parentItem = static_cast<TreeItem *> (parent.internalPointer ());
	}
	else
	{
		parentItem = m_rootItem;
	}

	TreeItem * childItem = parentItem->child (row);
	if (childItem)
	{
		return createIndex (row, column, childItem);
	}

	return {};
}

QModelIndex TreeModel::parent (const QModelIndex & index) const
{
	if (!index.isValid ())
	{
		return {};
	}

	auto childItem = static_cast<TreeItem *> (index.internalPointer ());
	TreeItem * parentItem = childItem->parentItem ();

	if (parentItem == m_rootItem)
	{
		return {};
	}

	return createIndex (parentItem->row (), 0, parentItem);
}

int TreeModel::rowCount (const QModelIndex & parent) const
{
	if (parent.column () > 0)
	{
		return 0;
	}

	TreeItem * parentItem;
	if (parent.isValid ())
	{
		parentItem = static_cast<TreeItem *> (parent.internalPointer ());
	}
	else
	{
		parentItem = m_rootItem;
	}

	return parentItem->childCount ();
}

int TreeModel::columnCount (const QModelIndex & parent) const
{
	Q_UNUSED (parent);
	return 1;
}


QHash<int, QByteArray> TreeModel::roleNames () const
{
	return m_roleNameMapping;
}

bool isPartOf (const kdb::Key & a, const kdb::Key & b)
{
	auto itA{ a.begin () };
	auto itB{ b.begin () };

	while (*(itA++) == *(itB++))
	{
		if (itA == a.end ())
		{
			return true;
		}
		if (itB == b.end ())
		{
			return false;
		}
	}
	return false;
}

void popEach (TreeItem ** currentTree, kdb::Key & currentKey)
{
	*currentTree = (**currentTree).parentItem ();
	(currentKey).delBaseName ();
}
void appendThis (TreeItem ** currentTree, kdb::Key & currentKey, const kdb::Key keyToAdd)
{
	// key already exists
	if (isPartOf (keyToAdd, currentKey))
	{
		return;
	}

	// If the branch of current key is deeper than we need to add
	while (!isPartOf (currentKey, keyToAdd))
	{
		popEach (currentTree, currentKey);
	}

	// If parent is not there, add it first
	kdb::Key parent = keyToAdd.dup ();
	parent.delBaseName ();
	if (currentKey != parent)
	{
		appendThis (currentTree, currentKey, parent);
	}


	// Let's add the key

	auto parentTree = *currentTree;
	*currentTree = new TreeItem{ keyToAdd, parentTree };
	parentTree->appendChild (*currentTree);

	currentKey = keyToAdd.dup ();
}


void TreeModel::setupModelData ()
{
	// TODO: Implement me!

	std::vector<std::string> roots{ "spec", "dir", "user", "system" };

	kdb::Key currentKey{ "/", KEY_END };
	auto currentTree = new TreeItem{ currentKey };
	m_rootItem = currentTree;


	for (auto root : roots)
	{
		kdb::KeySet keySet;
		kdb::Key rootKey{ root, KEY_END };
		currentKey = rootKey;
		currentTree = new TreeItem{ currentKey, m_rootItem };
		m_rootItem->appendChild (currentTree);


		m_kdb->get (keySet, rootKey);

		for (const kdb::Key & key : keySet)
		{
			appendThis (&currentTree, currentKey, key.dup ());
		}
	}
}


TreeItem* TreeModel::root () const
{
	return m_rootItem;
}


void TreeModel::refresh ()
{
	layoutAboutToBeChanged ();
	layoutChanged ();

	emit updateIndicator ();
}

void TreeModel::synchronize ()
{
	kdb::KeySet ours = collectCurrentKeySet ();

	try
	{
#if DEBUG && VERBOSE
		std::cout << "guitest: start" << std::endl;
		printKeys (ours, ours, ours);
#endif

		kdb::tools::merging::ThreeWayMerge merger;
		kdb::tools::merging::AutoMergeConfiguration configuration;
		configuration.configureMerger (merger);

		// write our config
		m_kdb->synchronize (ours,{"/", KEY_END}, merger);

#if DEBUG && VERBOSE
		std::cout << "guitest: after get" << std::endl;
		printKeys (ours, ours, ours);
#endif

//		createNewNodes (ours);
	}
	catch (kdb::tools::merging::MergingKDBException const & exc)
	{
//		QStringList conflicts = getConflicts (exc.getConflicts ());
//		emit showMessage (tr ("Error"), tr ("Synchronizing failed, conflicts occurred."), conflicts.join ("\n"));
	}
	catch (kdb::KDBException const & e)
	{
//		QMap<QString, QString> message = getErrorMessage (e);

//		emit showMessage (
//			tr ("Error"), message.value ("error"),
//			QString ("%1%2%3").arg (message.value ("at"), message.value ("mountpoint"), message.value ("configfile")));
	}
}

void TreeModel::exportConfiguration (const QModelIndex &index, QString format, QString file)
{
	auto parentModel = static_cast<TreeItem*>(index.internalPointer ());

	kdb::KeySet ks = collectCurrentKeySet ();
	kdb::Key root = parentModel->key ();

	// Node is only a filler
	if (!root) {
		root = kdb::Key (parentModel->path ().toStdString (), KEY_END);
	}

	kdb::KeySet part (ks.cut (root));

	std::string formatString = format.toStdString ();
	std::string fileString = file.remove ("file://").toStdString ();


	kdb::tools::Modules modules;
	kdb::tools::PluginPtr plugin = modules.load (formatString);

	kdb::Key errorKey (root);
	errorKey.setString (fileString);

	plugin->set (part, errorKey);

	std::stringstream ws;
	std::stringstream es;
	QString warnings;
	QString errors;

	printWarnings (ws, errorKey, true, true);
	warnings = QString::fromStdString (ws.str ());
	printError (es, errorKey, true, true);
	errors = QString::fromStdString (es.str ());

	if (errors.isEmpty () && !warnings.isEmpty ())
		emit showMessage (tr ("Information"),
				  tr ("Successfully exported configuration below %1 to %2, warnings were issued.")
					  .arg (QString::fromStdString (root.getName ()), file),
				  "");
	else if (!errors.isEmpty () && warnings.isEmpty ())
		emit showMessage (
			tr ("Error"),
			tr ("Failed to export configuration below %1 to %2.").arg (QString::fromStdString (root.getName ()), file), errors);
	else if (!errors.isEmpty () && !warnings.isEmpty ())
		emit showMessage (
			tr ("Error"),
			tr ("Failed to export configuration below %1 to %2.").arg (QString::fromStdString (root.getName ()), file),
			warnings + "\n" + errors);
}

kdb::KeySet TreeModel::collectCurrentKeySet ()
{
	KeySetVisitor ksVisit;
	accept (ksVisit);

	return ksVisit.getKeySet ();
}

void TreeModel::accept (Visitor & visitor)
{
	visitor.visit (this);
}
