struct Scope;
struct Checker;
struct Type;
enum BuiltinProcId;
enum ImplicitValueId;

#define ENTITY_KINDS \
	ENTITY_KIND(Invalid) \
	ENTITY_KIND(Constant) \
	ENTITY_KIND(Variable) \
	ENTITY_KIND(TypeName) \
	ENTITY_KIND(Procedure) \
	ENTITY_KIND(Builtin) \
	ENTITY_KIND(ImportName) \
	ENTITY_KIND(Nil) \
	ENTITY_KIND(ImplicitValue) \
	ENTITY_KIND(Count)

enum EntityKind {
#define ENTITY_KIND(k) GB_JOIN2(Entity_, k),
	ENTITY_KINDS
#undef ENTITY_KIND
};

String const entity_strings[] = {
#define ENTITY_KIND(k) {cast(u8 *)#k, gb_size_of(#k)-1},
	ENTITY_KINDS
#undef ENTITY_KIND
};

enum EntityFlag : u32 {
	EntityFlag_Visited    = 1<<0,
	EntityFlag_Used       = 1<<1,
	EntityFlag_Anonymous  = 1<<2,
	EntityFlag_Field      = 1<<3,
	EntityFlag_Param      = 1<<4,
	EntityFlag_VectorElem = 1<<5,
};

struct Entity {
	EntityKind kind;
	u32        flags;
	Token      token;
	Scope *    scope;
	Type *     type;
	AstNode *  identifier; // Can be NULL

	// TODO(bill): Cleanup how `using` works for entities
	Entity *   using_parent;
	AstNode *  using_expr;

	union {
		struct {
			ExactValue value;
		} Constant;
		struct {
			i32 field_index;
			i32 field_src_index;
		} Variable;
		struct {} TypeName;
		struct {} Procedure;
		struct {
			BuiltinProcId id;
		} Builtin;
		struct {
			String path;
			String name;
			Scope *scope;
			b32    used;
		} ImportName;
		struct {} Nil;
		struct {
			// TODO(bill): Should this be a user-level construct rather than compiler-level?
			ImplicitValueId id;
			Entity *        backing;
		} ImplicitValue;
	};
};

b32 is_entity_exported(Entity *e) {
	if (e->kind == Entity_ImportName) {
		return false;
	}
	// TODO(bill): Do I really want non-exported entities?
	// TODO(bill): If we do, what should be the rules?
	// if (e->token.string.len >= 1 &&
	    // e->token.string.text[0] == '_') {
		// return false;
	// }
	return true;
}


Entity *alloc_entity(gbAllocator a, EntityKind kind, Scope *scope, Token token, Type *type) {
	Entity *entity = gb_alloc_item(a, Entity);
	entity->kind   = kind;
	entity->scope  = scope;
	entity->token  = token;
	entity->type   = type;
	return entity;
}

Entity *make_entity_variable(gbAllocator a, Scope *scope, Token token, Type *type) {
	Entity *entity = alloc_entity(a, Entity_Variable, scope, token, type);
	return entity;
}

Entity *make_entity_using_variable(gbAllocator a, Entity *parent, Token token, Type *type) {
	GB_ASSERT(parent != NULL);
	Entity *entity = alloc_entity(a, Entity_Variable, parent->scope, token, type);
	entity->using_parent = parent;
	entity->flags |= EntityFlag_Anonymous;
	return entity;
}


Entity *make_entity_constant(gbAllocator a, Scope *scope, Token token, Type *type, ExactValue value) {
	Entity *entity = alloc_entity(a, Entity_Constant, scope, token, type);
	entity->Constant.value = value;
	return entity;
}

Entity *make_entity_type_name(gbAllocator a, Scope *scope, Token token, Type *type) {
	Entity *entity = alloc_entity(a, Entity_TypeName, scope, token, type);
	return entity;
}

Entity *make_entity_param(gbAllocator a, Scope *scope, Token token, Type *type, b32 anonymous) {
	Entity *entity = make_entity_variable(a, scope, token, type);
	entity->flags |= EntityFlag_Used;
	entity->flags |= EntityFlag_Anonymous*(anonymous != 0);
	entity->flags |= EntityFlag_Param;
	return entity;
}

Entity *make_entity_field(gbAllocator a, Scope *scope, Token token, Type *type, b32 anonymous, i32 field_src_index) {
	Entity *entity = make_entity_variable(a, scope, token, type);
	entity->Variable.field_src_index = field_src_index;
	entity->Variable.field_index = field_src_index;
	entity->flags |= EntityFlag_Field;
	entity->flags |= EntityFlag_Anonymous*(anonymous != 0);
	return entity;
}

Entity *make_entity_vector_elem(gbAllocator a, Scope *scope, Token token, Type *type, i32 field_src_index) {
	Entity *entity = make_entity_variable(a, scope, token, type);
	entity->Variable.field_src_index = field_src_index;
	entity->Variable.field_index = field_src_index;
	entity->flags |= EntityFlag_Field;
	entity->flags |= EntityFlag_VectorElem;
	return entity;
}

Entity *make_entity_procedure(gbAllocator a, Scope *scope, Token token, Type *signature_type) {
	Entity *entity = alloc_entity(a, Entity_Procedure, scope, token, signature_type);
	return entity;
}

Entity *make_entity_builtin(gbAllocator a, Scope *scope, Token token, Type *type, BuiltinProcId id) {
	Entity *entity = alloc_entity(a, Entity_Builtin, scope, token, type);
	entity->Builtin.id = id;
	return entity;
}

Entity *make_entity_import_name(gbAllocator a, Scope *scope, Token token, Type *type,
                                String path, String name, Scope *import_scope) {
	Entity *entity = alloc_entity(a, Entity_ImportName, scope, token, type);
	entity->ImportName.path = path;
	entity->ImportName.name = name;
	entity->ImportName.scope = import_scope;
	return entity;
}

Entity *make_entity_nil(gbAllocator a, String name, Type *type) {
	Token token = make_token_ident(name);
	Entity *entity = alloc_entity(a, Entity_Nil, NULL, token, type);
	return entity;
}

Entity *make_entity_implicit_value(gbAllocator a, String name, Type *type, ImplicitValueId id) {
	Token token = make_token_ident(name);
	Entity *entity = alloc_entity(a, Entity_ImplicitValue, NULL, token, type);
	entity->ImplicitValue.id = id;
	return entity;
}


Entity *make_entity_dummy_variable(gbAllocator a, Scope *file_scope, Token token) {
	token.string = str_lit("_");
	return make_entity_variable(a, file_scope, token, NULL);
}

