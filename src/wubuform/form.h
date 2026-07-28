/* form.h -- PDF form-field data model (EXP-82 partial). Stores AcroForm-style
 * fields (name, type, value) so a document can carry fillable form data even
 * before a full PDF AcroForm writer exists. Opaque. */
#ifndef WUBUFORM_H
#define WUBUFORM_H

typedef struct Form Form;

typedef enum { FORM_TEXT, FORM_CHECKBOX, FORM_CHOICE } FormType;

Form *form_create(void);
void  form_destroy(Form *f);

int   form_add(Form *f, const char *name, FormType type, const char *value); /* 1 */
int   form_set_value(Form *f, const char *name, const char *value);          /* 1 */
const char *form_value(const Form *f, const char *name);
FormType form_type(const Form *f, const char *name);
int   form_count(const Form *f);
const char *form_name_at(const Form *f, int i);

#endif /* WUBUFORM_H */
