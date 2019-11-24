#include "treeitem.hpp"

QString getNameFromKey (const kdb::Key & key)
{
	std::string name = key.getBaseName ();
	if (name.empty ())
	{
		name = key.getName ();
	}
	return QString::fromStdString (name);
}


TreeItem::TreeItem (const kdb::Key & key, TreeItem * parentItem)
{
	m_key = key;
	m_parentItem = parentItem;
	m_name = getNameFromKey (key);
	m_path = QString::fromStdString (key.getFullName ());
	m_isExpandable = false;
}

TreeItem::~TreeItem ()
{
	qDeleteAll (m_childItems);
}

QString TreeItem::name () const
{
	return m_name;
}

QString TreeItem::path () const
{
	return m_path;
}

kdb::Key TreeItem::key () const
{
	return m_key;
}

bool TreeItem::expandable () const
{
	return m_isExpandable;
}

void TreeItem::setExpandable (bool isExpandable)
{
	m_isExpandable = isExpandable;
}
void TreeItem::appendChild (TreeItem * child)
{
	m_childItems.append (child);
	if (m_parentItem)
	{
		m_parentItem->setExpandable (true);
	}
}

TreeItem * TreeItem::child (int row)
{
	if (row < 0 || row >= m_childItems.size ()) return nullptr;
	return m_childItems.at (row);
}

int TreeItem::childCount () const
{
	if (m_isExpandable)
	{
		return m_childItems.count ();
	}
	else
	{
		return 0;
	}
}

int TreeItem::columnCount () const
{
	return 1;
}

QVariant TreeItem::data (int column) const
{
	if (column < 0 || column >= columnCount ())
	{
		return {};
	}

	return m_name;
}

int TreeItem::row () const
{
	return m_parentItem->m_childItems.indexOf (const_cast<TreeItem *> (this));
}

TreeItem * TreeItem::parentItem ()
{
	return m_parentItem;
}

void TreeItem::accept (Visitor & visitor)
{
	visitor.visit (*this);
	for (auto child : m_childItems)
	{
		child->accept (visitor);
	}
}
