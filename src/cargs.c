#include <assert.h>
#include <cargs.h>
//#include <memory.h>
#include <stdio.h>
#include <string.h>

#define CAG_OPTION_PRINT_DISTANCE 4
#define CAG_OPTION_PRINT_MIN_INDENTION 20

static void cag_option_print_value(const cag_option *option,
  size_t *accessor_length, cag_printer printer, void *printer_ctx)
{
  if (option->value_name != NULL) {
    *accessor_length += printer(printer_ctx, "=%s", option->value_name);
  }
}

static void cag_option_print_letters(const cag_option *option, bool *first,
  size_t *accessor_length, cag_printer printer, void *printer_ctx)
{
  const char *access_letter;
  access_letter = option->access_letters;
  if (access_letter != NULL) {
    while (*access_letter) {
      if (*first) {
        *accessor_length += printer(printer_ctx, "-%c", *access_letter);
        *first = false;
      } else {
        *accessor_length += printer(printer_ctx, ", -%c", *access_letter);
      }
      ++access_letter;
    }
  }
}

static void cag_option_print_name(const cag_option *option, bool *first,
  size_t *accessor_length, cag_printer printer, void *printer_ctx)
{
  if (option->access_name != NULL) {
    if (*first) {
      *accessor_length += printer(printer_ctx, "--%s", option->access_name);
    } else {
      *accessor_length += printer(printer_ctx, ", --%s", option->access_name);
    }
  }
}

static size_t cag_option_get_print_indention(const cag_option *options,
  size_t option_count)
{
  size_t option_index, indention, result;
  const cag_option *option;

  result = CAG_OPTION_PRINT_MIN_INDENTION;

  for (option_index = 0; option_index < option_count; ++option_index) {
    indention = CAG_OPTION_PRINT_DISTANCE;
    option = &options[option_index];
    if (option->access_letters != NULL && *option->access_letters) {
      indention += strlen(option->access_letters) * 4 - 2;
      if (option->access_name != NULL) {
        indention += strlen(option->access_name) + 4;
      }
    } else if (option->access_name != NULL) {
      indention += strlen(option->access_name) + 2;
    }

    if (option->value_name != NULL) {
      indention += strlen(option->value_name) + 1;
    }

    if (indention > result) {
      result = indention;
    }
  }

  return result;
}

void cag_option_init(cag_option_context *context, const cag_option *options,
  size_t option_count, int argc, char **argv)
{
  // This just initialized the values to the beginning of all the arguments.
  context->options = options;
  context->option_count = option_count;
  context->argc = argc;
  context->argv = argv;
  context->index = 1;
  context->inner_index = 0;
  context->forced_end = false;
  context->error_index = -1;
  context->error_letter = 0;
}

static const cag_option *cag_option_find_by_name(cag_option_context *context,
  char *name, size_t name_size)
{
  const cag_option *option;
  size_t i;

  // We loop over all the available options and stop as soon as we have found
  // one. We don't use any hash map table, since there won't be that many
  // arguments anyway.
  for (i = 0; i < context->option_count; ++i) {
    option = &context->options[i];

    // The option might not have an item name, we can just skip those.
    if (option->access_name == NULL) {
      continue;
    }

    // Try to compare the name of the access name. We can use the name_size or
    // this comparison, since we are guaranteed to have null-terminated access
    // names.
    if (strncmp(option->access_name, name, name_size) == 0) {
      return option;
    }
  }

  return NULL;
}

static const cag_option *cag_option_find_by_letter(cag_option_context *context,
  char letter)
{
  const cag_option *option;
  size_t i;

  // We loop over all the available options and stop as soon as we have found
  // one. We don't use any look up table, since there won't be that many
  // arguments anyway.
  for (i = 0; i < context->option_count; ++i) {
    option = &context->options[i];

    // If this option doesn't have any access letters we will skip them.
    if (option->access_letters == NULL) {
      continue;
    }

    // Verify whether this option has the access letter in it's access letter
    // string. If it does, then this is our option.
    if (strchr(option->access_letters, letter) != NULL) {
      return option;
    }
  }

  return NULL;
}

static void cag_option_parse_value(cag_option_context *context,
  const cag_option *option, char **c)
{
  // And now let's check whether this option is supposed to have a value, which
  // is the case if there is a value name set. The value can be either submitted
  // with a '=' sign or a space, which means we would have to jump over to the
  // next argv index. This is somewhat ugly, but we do it to behave the same as
  // the other option parsers.
  if (option->value_name != NULL) {
    if (**c == '=') {
      context->value = ++(*c);
    } else {
      // If the next index is larger or equal to the argument count, then the
      // parameter for this option is missing. The user will know about this,
      // since the value pointer of the context will be NULL because we don't
      // set it here in that case.
      if (context->argc > context->index + 1) {
        // We consider this argv to be the value, no matter what the contents
        // are.
        ++context->index;
        *c = context->argv[context->index];
        context->value = *c;
      }
    }

    // Move c to the end of the value, to not confuse the caller about our
    // position.
    while (**c) {
      ++(*c);
    }
  }
}

static void cag_option_parse_access_name(cag_option_context *context, char **c)
{
  const cag_option *option;
  char *n;

  // Now we need to extract the access name, which is any symbol up to a '=' or
  // a '\0'.
  n = *c;
  while (**c && **c != '=') {
    ++*c;
  }

  // Now this will obviously always be true, but we are paranoid. Sometimes. It
  // doesn't hurt to check.
  assert(*c >= n);

  // Figure out which option this name belongs to. This might return NULL if the
  // name is not registered, which means the user supplied an unknown option. In
  // that case we return true to indicate that we finished with this option. We
  // have to skip the value parsing since we don't know whether the user thinks
  // this option has one or not. Since we don't set any identifier specifically,
  // it will remain '?' within the context.
  option = cag_option_find_by_name(context, n, (size_t)(*c - n));
  if (option != NULL) {
    // We found an option and now we can specify the identifier within the
    // context.
    context->identifier = option->identifier;

    // And now we try to parse the value. This function will also check whether
    // this option is actually supposed to have a value.
    cag_option_parse_value(context, option, c);
  } else {
    // Remember the error index so that we can print a error message.
    context->error_index = context->index;
  }

  // And finally we move on to the next index.
  ++context->index;
}

static void cag_option_parse_access_letter(cag_option_context *context,
  char **c)
{
  const cag_option *option;
  char *n, *v, letter;

  n = *c;

  // Figure out which option this letter belongs to. This might return NULL if
  // the letter is not registered, which means the user supplied an unknown
  // option. In that case we return true to indicate that we finished with this
  // option. We have to skip the value parsing since we don't know whether the
  // user thinks this option has one or not. Since we don't set any identifier
  // specifically, it will remain '?' within the context.
  letter = n[context->inner_index];
  option = cag_option_find_by_letter(context, letter);
  v = &n[++context->inner_index];
  if (option == NULL) {
    context->error_index = context->index;
    context->error_letter = letter;
  } else {
    // We found an option and now we can specify the identifier within the
    // context.
    context->identifier = option->identifier;

    // And now we try to parse the value. This function will also check whether
    // this option is actually supposed to have a value.
    cag_option_parse_value(context, option, &v);
  }

  // Check whether we reached the end of this option argument.
  if (*v == '\0') {
    ++context->index;
    context->inner_index = 0;
  }
}

static void cag_option_shift(cag_option_context *context, int start, int option,
  int end)
{
  char *tmp;
  int a_index, shift_index, left_shift, right_shift, target_index, source_index;

  // The block between start and option will be shifted to the end, and the order
  // of everything will be preserved. Left shift is the amount of indexes the block
  // between option and end will shift towards the start, and right shift is the
  // amount of indexes the block between start and option will be shifted towards
  // the end.
  left_shift = option - start;
  right_shift = end - option;

  // There is no shift is required if the start and the option have the same
  // index.
  if (left_shift == 0) {
    return;
  }

  // Let's loop through the option strings first, which we will move towards the
  // beginning.
  for (a_index = option; a_index < end; ++a_index) {
    // First remember the current option value, because we will have to save
    // that later at the beginning.
    tmp = context->argv[a_index];

    // Let's loop over all option values and shift them one towards the end.
    // This will override the option value we just stored temporarily.
    for (shift_index = 0; shift_index < left_shift; ++shift_index) {
      target_index = a_index - shift_index;
      source_index = a_index - shift_index - 1;
      context->argv[target_index] = context->argv[source_index];
    }

    // Now restore the saved option value at the beginning.
    context->argv[a_index - left_shift] = tmp;
  }

  // The new index will be before all non-option values, in such a way that they
  // all will be moved again in the next fetch call.
  context->index = end - left_shift;

  // The error index may have changed, we need to fix that as well.
  if (context->error_index >= start) {
    if (context->error_index < option) {
      context->error_index += right_shift;
    } else if (context->error_index < end) {
      context->error_index -= left_shift;
    }
  }
}

static bool cag_option_is_argument_string(const char *c)
{
  return *c == '-' && *(c + 1) != '\0';
}

static int cag_option_find_next(cag_option_context *context)
{
  // Prepare to search the next option at the next index.
  int next_index;
  char *c;

  next_index = context->index;

  // Let's verify that it is not the end If it is
  // the end we have to return -1 to indicate that we finished.
  if (next_index >= context->argc) {
    return -1;
  }

  // Grab a pointer to the argument string.
  c = context->argv[next_index];
  if (context->forced_end || c == NULL) {
    return -1;
  }

  // Check whether it is a '-'. We need to find the next option - and an option
  // always starts with a '-'. If there is a string "-\0", we don't consider it
  // as an option neither.
  while (!cag_option_is_argument_string(c)) {
    if (++next_index >= context->argc) {
      // We reached the end and did not find any argument anymore. Let's tell
      // our caller that we reached the end.
      return -1;
    }

    c = context->argv[next_index];
    if (c == NULL) {
      return -1;
    }
  }

  // Indicate that we found an option which can be processed. The index of the
  // next option will be returned.
  return next_index;
}

bool cag_option_fetch(cag_option_context *context)
{
  char *c;
  int old_index, new_index;

  // Reset our identifier to a question mark, which indicates an "unknown"
  // option. The value is set to NULL, to make sure we are not carrying the
  // parameter from the previous option to this one.
  context->identifier = '?';
  context->value = NULL;
  context->error_index = -1;
  context->error_letter = 0;

  // Check whether there are any options left to parse and remember the old
  // index as well as the new index. In the end we will move the option junk to
  // the beginning, so that non option arguments can be read.
  old_index = context->index;
  new_index = cag_option_find_next(context);
  if (new_index >= 0) {
    context->index = new_index;
  } else {
    return false;
  }

  // Grab a pointer to the beginning of the option. At this point, the next
  // character must be a '-', since if it was not the prepare function would
  // have returned false. We will skip that symbol and proceed.
  c = context->argv[context->index];
  assert(*c == '-');
  ++c;

  // Check whether this is a long option, starting with a double "--".
  if (*c == '-') {
    ++c;

    // This might be a double "--" which indicates the end of options. If this
    // is the case, we will not move to the next index. That ensures that
    // another call to the fetch function will not skip the "--".
    if (*c == '\0') {
      context->forced_end = true;
    } else {
      // We parse now the access name. All information about it will be written
      // to the context.
      cag_option_parse_access_name(context, &c);
    }
  } else {
    // This is no long option, so we can just parse an access letter.
    cag_option_parse_access_letter(context, &c);
  }

  // Move the items so that the options come first followed by non-option
  // arguments.
  cag_option_shift(context, old_index, new_index, context->index);

  return context->forced_end == false;
}

char cag_option_get_identifier(const cag_option_context *context)
{
  // We just return the identifier here.
  return context->identifier;
}

const char *cag_option_get_value(const cag_option_context *context)
{
  // We just return the internal value pointer of the context.
  return context->value;
}

int cag_option_get_index(const cag_option_context *context)
{
  // Either we point to a value item,
  return context->index;
}

CAG_PUBLIC int cag_option_get_error_index(const cag_option_context *context)
{
  // This is set
  return context->error_index;
}

CAG_PUBLIC char cag_option_get_error_letter(const cag_option_context *context)
{
  // This is set to the unknown option letter if it was parsed.
  return context->error_letter;
}

CAG_PUBLIC void cag_option_printer_error(const cag_option_context *context,
  cag_printer printer, void *printer_ctx)
{
  int error_index;
  char error_letter;

  error_index = cag_option_get_error_index(context);
  if (error_index < 0) {
    return;
  }

  error_letter = cag_option_get_error_letter(context);
  if (error_letter) {
    printer(printer_ctx, "Unknown option '%c' in '%s'.\n", error_letter,
      context->argv[error_index]);
  } else {
    printer(printer_ctx, "Unknown option '%s'.\n", context->argv[error_index]);
  }
}

#ifndef CAG_NO_FILE
CAG_PUBLIC void cag_option_print_error(const cag_option_context* context,
    FILE* destination)
{
  cag_option_printer_error(context, (cag_printer)fprintf, destination);
}
#endif

#ifndef CAG_NO_FILE
void cag_option_print(const cag_option* options, size_t option_count,
    FILE* destination)
{
  cag_option_printer(options, option_count, (cag_printer)fprintf, destination);
}
#endif

CAG_PUBLIC void cag_option_printer(const cag_option *options,
  size_t option_count, cag_printer printer, void *printer_ctx)
{
  size_t option_index, indention, i, accessor_length;
  const cag_option *option;
  bool first;

  indention = cag_option_get_print_indention(options, option_count);

  for (option_index = 0; option_index < option_count; ++option_index) {
    option = &options[option_index];
    accessor_length = 0;
    first = true;

    printer(printer_ctx, "  ");

    cag_option_print_letters(option, &first, &accessor_length, printer,
      printer_ctx);
    cag_option_print_name(option, &first, &accessor_length, printer,
      printer_ctx);
    cag_option_print_value(option, &accessor_length, printer, printer_ctx);

    for (i = accessor_length; i < indention; ++i) {
      printer(printer_ctx, " ");
    }

    printer(printer_ctx, " %s\n", option->description);
  }
}

void cag_option_prepare(cag_option_context *context, const cag_option *options,
  size_t option_count, int argc, char **argv)
{
  cag_option_init(context, options, option_count, argc, argv);
}

char cag_option_get(const cag_option_context *context)
{
  return cag_option_get_identifier(context);
}
