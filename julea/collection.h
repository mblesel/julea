#ifndef H_COLLECTION
#define H_COLLECTION

struct JCollection;

typedef struct JCollection JCollection;

#include <glib.h>

#include "semantics.h"

JCollection* j_collection_new (const gchar*);
JCollection* j_collection_ref (JCollection*);
void j_collection_unref (JCollection*);

void j_collection_create (JCollection*, GList*);

void j_collection_set_semantics (JCollection*, JSemantics*);

/*
#include "credentials.h"
#include "item.h"
#include "public.h"
#include "ref_counted.h"
#include "semantics.h"
#include "store.h"

namespace JULEA
{
	class _Collection : public RefCounted<_Collection>
	{
		friend class RefCounted<_Collection>;

		friend class Collection;

		friend class _Item;
		friend class _Store;

		public:
			string const& Name () const;

			std::list<Item> Get (std::list<string>);

			_Semantics const* GetSemantics ();
		private:
			_Collection (_Store*, mongo::BSONObj const&);
			~_Collection ();

			void IsInitialized (bool) const;

			mongo::BSONObj Serialize ();
			void Deserialize (mongo::BSONObj const&);

			std::string const& ItemsCollection ();

			mongo::OID const& ID () const;

			void Associate (_Store*);

			bool m_initialized;

			mongo::OID m_id;
			string m_name;

			Credentials m_owner;

			_Semantics* m_semantics;
			_Store* m_store;

			std::string m_itemsCollection;
	};

	class Collection : public Public<_Collection>
	{
		friend class _Store;

		public:
			class Iterator
			{
				public:
					Iterator (Collection const&);
					~Iterator ();

					bool More ();
					Item Next ();
				private:
					mongo::ScopedDbConnection* m_connection;
					_Collection* m_collection;
					std::auto_ptr<mongo::DBClientCursor> m_cursor;
			};

		public:
			Collection (string const& name)
			{
				m_p = new _Collection(name);
			}

		private:
			Collection (_Store* store, mongo::BSONObj const& obj)
			{
				m_p = new _Collection(store, obj);
			}
	};
}
*/

#endif
