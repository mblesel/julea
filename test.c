#include <glib.h>

#include "julea.h"

int main (int argc, char** argv)
{
	JConnection* connection;
	JStore* store;
	JSemantics* semantics;
	GList* collections = NULL;
	GList* items = NULL;

	if (argc != 2)
	{
		return 1;
	}

	connection = j_connection_new();

	if (!j_connection_connect(connection, argv[1]))
	{
		return 1;
	}

	store = j_connection_get(connection, "JULEA");

	semantics = j_semantics_new();
	j_semantics_set(semantics, J_SEMANTICS_PERSISTENCY, J_SEMANTICS_PERSISTENCY_LAX);

	for (guint j = 0; j < 10; j++)
	{
		JCollection* collection;
		gchar* name;

		name = g_strdup_printf("test-%u", j);

		collection = j_collection_new(name);
		j_collection_set_semantics(collection, semantics);

		collections = g_list_prepend(collections, collection);

		g_free(name);
	}

	j_store_create(store, collections);

	/*
	list<Collection>::iterator it;

	for (it = collections.begin(); it != collections.end(); ++it)
	{
		for (int i = 0; i < 10000; i++)
		{
			string name("test-" + lexical_cast<string>(i));
			Item item(name);

	//		item->SetSemantics(semantics);
			items.push_back(item);
		}

		(*it)->Create(items);
	}

	Collection::Iterator iterator(collections.front());

	while (iterator.More())
	{
		Item i = iterator.Next();

//		cout << i->Name() << endl;
	}
	*/

	j_semantics_unref(semantics);
	j_store_unref(store);
	j_connection_unref(connection);

	return 0;
}
