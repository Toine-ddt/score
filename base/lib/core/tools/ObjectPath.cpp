#include <QApplication>
#include <core/tools/IdentifiedObject.hpp>
#include <core/tools/ObjectPath.hpp>

ObjectPath ObjectPath::pathFromObject(QString parent_name, QObject* origin_object)
{
	QVector<ObjectIdentifier> v;
	QObject* parent_obj = qApp->findChild<QObject*>(parent_name);

	auto current_obj = origin_object;
	auto add_parent_to_vector = [&v] (QObject* ptr)
	{
		if(auto id_obj = dynamic_cast<IdentifiedObject*>(ptr))
			v.push_back({id_obj->objectName(), id_obj->id()});
		else
			v.push_back({ptr->objectName(), {}});
	};

	// Recursively go through the object and all the parents
	while(current_obj != parent_obj)
	{
		if(current_obj->objectName().isEmpty())
		{
			throw std::runtime_error("ObjectPath::pathFromObject : an object in the hierarchy does not have a name.");
		}

		add_parent_to_vector(current_obj);

		current_obj = current_obj->parent();
		if(!current_obj)
		{
			throw std::runtime_error("ObjectPath::pathFromObject : Could not find path to parent object");
		}
	}

	// Add the last parent (the one specified with parent_name)
	add_parent_to_vector(current_obj);

	// Search goes from top to bottom (of the parent hierarchy) instead
	std::reverse(std::begin(v), std::end(v));
	return std::move(v);
}

QObject* ObjectPath::find() const
{
	auto parent_name = m_objectIdentifiers.at(0).childName();
	std::vector<ObjectIdentifier> children(m_objectIdentifiers.size() - 1);
	std::copy(std::begin(m_objectIdentifiers) + 1,
			  std::end(m_objectIdentifiers),
			  std::begin(children));

	QObject* obj = qApp->findChild<QObject*>(parent_name);

	for(const auto& currentObjIdentifier : children)
	{
		if(currentObjIdentifier.id().set())
		{
			auto childs = obj->findChildren<IdentifiedObject*>(currentObjIdentifier.childName(),
																Qt::FindDirectChildrenOnly);

			auto elt = findById(childs, currentObjIdentifier.id());
			if(!elt)
			{
				throw std::runtime_error("ObjectPath::find  Error! Child not found");
			}

			obj = elt;
		}
		else
		{
			auto child = obj->findChild<NamedObject*>(currentObjIdentifier.childName(),
													  Qt::FindDirectChildrenOnly);
			if(!child)
			{
				throw std::runtime_error("ObjectPath::find  Error! Child not found");
			}

			obj = child;
		}
	}

	return obj;
}
