/* vars.h -- named document variables (DOC-73). A variable map (name -> value)
 * resolved into text (${name}) for script fields / template expansion. Opaque. */
#ifndef WUBUVARS_H
#define WUBUVARS_H

typedef struct Vars Vars;

Vars *vars_create(void);
void  vars_destroy(Vars *v);

/* Set/overwrite a variable. Returns 1 on success. */
int   vars_set(Vars *v, const char *name, const char *value);
/* Get a variable value (NULL if unset). Do NOT free. */
const char *vars_get(const Vars *v, const char *name);

/* Expand ${name} occurrences in `text` using the map; returns a malloc'd string
 * (caller frees) with unknowns left as "${name}". */
char *vars_expand(const Vars *v, const char *text);

int   vars_count(const Vars *v);
const char *vars_name_at(const Vars *v, int i);

#endif /* WUBUVARS_H */
