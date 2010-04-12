/*
 * s390-tools/zipl/src/scan.c
 *   Scanner for zipl.conf configuration files
 *
 * Copyright IBM Corp. 2001, 2009.
 *
 * Author(s): Carsten Otte <cotte@de.ibm.com>
 *            Peter Oberparleiter <Peter.Oberparleiter@de.ibm.com>
 */

#include "scan.h"

/* Need ISOC99 function isblank() in ctype.h */
#ifndef __USE_ISOC99
#define __USE_ISOC99
#endif

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "misc.h"
#include "error.h"


/* Determines which keyword may be present in which section */
enum scan_key_state scan_key_table[SCAN_SECTION_NUM][SCAN_KEYWORD_NUM] = {
/*	 defa dump dump imag para parm ramd segm targ prom time defa tape mv
 *	 ult  to   tofs e    mete file isk  ent  et   pt   out  ultm      dump
 *			     rs                                 enu
 *
 *       targ targ targ targ targ
 *       etba etty etge etbl etof
 *       se   pe   omet ocks fset
 *                 ry   ize
 */
/* defaultboot	*/
	{opt, inv, inv, inv, inv, inv, inv, inv, inv, inv, inv, opt, inv, inv,
	 inv, inv, inv, inv, inv},
/* ipl		*/
	{inv, inv, inv, req, opt, opt, opt, inv, req, inv, inv, inv, inv, inv,
	 opt, opt, opt, opt, opt},
/* segment load */
	{inv, inv, inv, inv, inv, inv, inv, req, req, inv, inv, inv, inv, inv,
	 inv, inv, inv, inv, inv},
/* part dump	*/
	{inv, req, inv, inv, inv, inv, inv, inv, opt, inv, inv, inv, inv, inv,
	 inv, inv, inv, inv, inv},
/* fs dump	*/
	{inv, inv, req, inv, opt, opt, inv, inv, req, inv, inv, inv, inv, inv,
	 inv, inv, inv, inv, inv},
/* ipl tape	*/
	{inv, inv, inv, req, opt, opt, opt, inv, inv, inv, inv, inv, req, inv,
	 inv, inv, inv, inv, inv},
/* multi volume dump	*/
	{inv, inv, inv, inv, inv, inv, inv, inv, inv, inv, inv, inv, inv, req,
	 inv, inv, inv, inv, inv}
};

/* Mapping of keyword IDs to strings */
static const struct {
	char* keyword;
	enum scan_keyword_id id;
} keyword_list[] = {
	{ "defaultmenu", scan_keyword_defaultmenu},
	{ "default", scan_keyword_default },
	{ "dumptofs", scan_keyword_dumptofs },
	{ "dumpto", scan_keyword_dumpto },
	{ "image", scan_keyword_image },
	{ "mvdump", scan_keyword_mvdump },
	{ "parameters", scan_keyword_parameters },
	{ "parmfile", scan_keyword_parmfile },
	{ "ramdisk", scan_keyword_ramdisk },
	{ "segment", scan_keyword_segment },
	{ "targetbase", scan_keyword_targetbase},
	{ "targettype", scan_keyword_targettype},
	{ "targetgeometry", scan_keyword_targetgeometry},
	{ "targetblocksize", scan_keyword_targetblocksize},
	{ "targetoffset", scan_keyword_targetoffset},
	{ "target", scan_keyword_target},
	{ "prompt", scan_keyword_prompt},
	{ "timeout", scan_keyword_timeout},
	{ "tape", scan_keyword_tape}
};

/* Retrieve name of keyword identified by ID. */
char *
scan_keyword_name(enum scan_keyword_id id)
{
	unsigned int i;

	for (i=0; i < sizeof(keyword_list) / sizeof(keyword_list[0]); i++)
		if (id == keyword_list[i].id)
			return keyword_list[i].keyword;
	return "<unknown>";
}


/* Advance the current file pointer of file buffer FILE until the current
 * character is no longer a blank. Return 0 if at least one blank
 * character was encountered, non-zero otherwise. */
static int
skip_blanks(struct misc_file_buffer* file)
{
	int rc;

	rc = -1;
	for (; isblank(misc_get_char(file, 0)); file->pos++)
		rc = 0;
	return rc;
}


/* Advance the current file position to beginning of next line in file buffer
 * FILE or to end of file. */
static void
skip_line(struct misc_file_buffer* file)
{
	for (;; file->pos++) {
		switch (misc_get_char(file, 0)) {
		case '\n':
			file->pos++;
			return;
		case EOF:
			return;
		}
	}
}


/* Skip trailing blanks of line. On success, return zero and set the file
 * buffer position to beginning of next line or EOF. Return non-zero if
 * non-blank characters were found before end of line. */
static int
skip_trailing_blanks(struct misc_file_buffer* file)
{
	int current;

	for (;; file->pos++) {
		current = misc_get_char(file, 0);
		if (current == '\n') {
			file->pos++;
			return 0;
		} else if (current == EOF)
			return 0;
		else if (!isblank(current))
			return -1;
	}
}


static int
scan_section_heading(struct misc_file_buffer* file, struct scan_token* token,
		     int line)
{
	int start_pos;
	int end_pos;
	int current;
	char* name;

	for (start_pos=file->pos; misc_get_char(file, 0) != ']'; file->pos++) {
		current = misc_get_char(file, 0);
		switch (current) {
		case EOF:
		case '\n':
			error_reason("Line %d: unterminated section heading",
				     line);
			return -1;
		default:
			if (!(isalnum(current) || ispunct(current))) {
				error_reason("Line %d: invalid character in "
					     "section name", line);
				return -1;
			}
		}
	}
	end_pos = file->pos;
	if (end_pos == start_pos) {
		error_reason("Line %d: empty section name", line);
		return -1;
	}
	file->pos++;
	if (skip_trailing_blanks(file)) {
		error_reason("Line %d: unexpected characters after section "
			     "name", line);
		return -1;
	}
	name = (char *) misc_malloc(end_pos - start_pos + 1);
	if (name == NULL)
		return -1;
	memcpy(name, &file->buffer[start_pos], end_pos - start_pos);
	name[end_pos - start_pos] = 0;
	token->id = scan_id_section_heading;
	token->line = line;
	token->content.section.name = name;
	return 0;
}


static int
scan_menu_heading(struct misc_file_buffer* file, struct scan_token* token,
		  int line)
{
	int start_pos;
	int end_pos;
	int current;
	char* name;

	for (start_pos=file->pos; ; file->pos++) {
		current = misc_get_char(file, 0);
		if ((current == EOF) || (current == '\n'))
			break;
		else if (isblank(current))
			break;
		else if (!isalnum(current)) {
			error_reason("Line %d: invalid character in menu name ",
				     line);
			return -1;
		}
	}
	end_pos = file->pos;
	if (skip_trailing_blanks(file)) {
		error_reason("Line %d: blanks not allowed in menu name",
			     line);
		return -1;
	}
	if (end_pos == start_pos) {
		error_reason("Line %d: empty menu name", line);
		return -1;
	}
	name = (char *) misc_malloc(end_pos - start_pos + 1);
	if (name == NULL)
		return -1;
	memcpy(name, &file->buffer[start_pos], end_pos - start_pos);
	name[end_pos - start_pos] = 0;
	token->id = scan_id_menu_heading;
	token->line = line;
	token->content.menu.name = name;
	return 0;
}


static int
scan_number(struct misc_file_buffer* file, int* number, int line)
{
	int start_pos;
	int old_number;
	int new_number;

	old_number = 0;
	new_number = 0;
	start_pos = file->pos;
	for (; isdigit(misc_get_char(file, 0)); file->pos++) {
		new_number = old_number*10 + misc_get_char(file, 0) - '0';
		if (new_number < old_number) {
			error_reason("Line %d: number too large", line);
			return -1;
		}
		old_number = new_number;
	}
	if (file->pos == start_pos) {
		error_reason("Line %d: number expected", line);
		return -1;
	}
	*number = new_number;
	return 0;
}


static int
scan_value_string(struct misc_file_buffer* file, char** value, int line)
{
	int quote;
	int start_pos;
	int end_pos;
	int last_nonspace;
	int current;
	char* string;

	current = misc_get_char(file, 0);
	if (current == '\"') {
		quote = '\"';
		file->pos++;
	} else if (current == '\'') {
		quote = '\'';
		file->pos++;
	} else quote = 0;
	last_nonspace = -1;
	for (start_pos=file->pos;; file->pos++) {
		current = misc_get_char(file, 0);
		if ((current == EOF) || (current == '\n')) {
			break;
		} else if (quote) {
			if (current == quote)
				break;
		} else if (!isblank(current))
			last_nonspace = file->pos;
	}
	end_pos = file->pos;
	if (quote) {
		if (current != quote) {
			error_reason("Line %d: unterminated quotes", line);
			return -1;
		}
	} else if (last_nonspace >= 0)
		end_pos = last_nonspace + 1;
	string = (char *) misc_malloc(end_pos - start_pos + 1);
	if (string == NULL)
		return -1;
	if (end_pos > start_pos)
		memcpy(string, &file->buffer[start_pos], end_pos - start_pos);
	string[end_pos - start_pos] = 0;
	*value = string;
	if (quote)
		file->pos++;
	return 0;
}


static int
scan_number_assignment(struct misc_file_buffer* file, struct scan_token* token,
		      int line)
{
	int rc;

	rc = scan_number(file, &token->content.number.number, line);
	if (rc)
		return rc;
	skip_blanks(file);
	if (misc_get_char(file, 0) != '=') {
		error_reason("Line %d: number expected as keyword", line);
		return -1;
	}
	file->pos++;
	skip_blanks(file);
	rc = scan_value_string(file, &token->content.number.value, line);
	if (rc)
		return rc;
	if (skip_trailing_blanks(file)) {
		error_reason("Line %d: unexpected characters at end of line",
			     line);
		return -1;
	}
	token->id = scan_id_number_assignment;
	token->line = line;
	return 0;
}

static int
match_keyword(struct misc_file_buffer* file, const char* keyword)
{
	unsigned int i;

	for (i=0; i<strlen(keyword); i++)
		if (misc_get_char(file, i) != keyword[i])
			return -1;
	return 0;
}


static int
scan_keyword(struct misc_file_buffer* file, enum scan_keyword_id* id, int line)
{
	unsigned int i;

	for (i=0; i < sizeof(keyword_list) / sizeof(keyword_list[0]); i++)
		if (match_keyword(file, keyword_list[i].keyword) == 0) {
			file->pos += strlen(keyword_list[i].keyword);
			*id = keyword_list[i].id;
			return 0;
		}
	error_reason("Line %d: unknown keyword", line);
	return -1;
}


static int
scan_keyword_assignment(struct misc_file_buffer* file, struct scan_token* token,
		       int line)
{
	int rc;

	rc = scan_keyword(file, &token->content.keyword.keyword, line);
	if (rc)
		return rc;
	skip_blanks(file);
	if (misc_get_char(file, 0) != '=') {
		error_reason("Line %d: unexpected characters after keyword",
			     line);
		return -1;
	}
	file->pos++;
	skip_blanks(file);
	rc = scan_value_string(file, &token->content.keyword.value, line);
	if (rc)
		return rc;
	if (skip_trailing_blanks(file)) {
		error_reason("Line %d: unexpected characters at end of line",
			     line);
		return -1;
	}
	token->id = scan_id_keyword_assignment;
	token->line = line;
	return 0;
}



static int
search_line_for(struct misc_file_buffer* file, int search)
{
	int i;
	int current;

	for (i=0; ; i++) {
		current = misc_get_char(file, i);
		switch (current) {
		case EOF:
		case '\n':
			return 0;
		default:
			if (current == search)
				return 1;
		}
	}
}


void
scan_free(struct scan_token* array)
{
	int i;

	for (i=0; array[i].id != 0; i++) {
		switch (array[i].id) {
		case scan_id_section_heading:
			if (array[i].content.section.name != NULL) {
				free(array[i].content.section.name);
				array[i].content.section.name = NULL;
			}
			break;
		case scan_id_menu_heading:
			if (array[i].content.menu.name != NULL) {
				free(array[i].content.menu.name);
				array[i].content.menu.name = NULL;
			}
			break;
		case scan_id_keyword_assignment:
			if (array[i].content.keyword.value != NULL) {
				free(array[i].content.keyword.value);
				array[i].content.keyword.value = NULL;
			}
			break;
		case scan_id_number_assignment:
			if (array[i].content.number.value != NULL) {
				free(array[i].content.number.value);
				array[i].content.number.value = NULL;
			}
			break;
		default:
			break;
		}
	}
	free(array);
}


#define INITIAL_ARRAY_LENGTH 40

/* Scan file FILENAME for tokens. Upon success, return zero and set TOKEN
 * to point to a NULL-terminated array of scan_tokens, i.e. the token id
 * of the last token is 0. Return non-zero otherwise. */
int
scan_file(const char* filename, struct scan_token** token)
{
	struct misc_file_buffer file;
	struct scan_token* array;
	struct scan_token* buffer;
	int pos;
	int size;
	int current;
	int rc;
	int line;

	rc = misc_get_file_buffer(filename, &file);
	if (rc)
		return rc;
	size = INITIAL_ARRAY_LENGTH;
	pos = 0;
	array = (struct scan_token*) misc_malloc(size *
						 sizeof(struct scan_token));
	if (array == NULL) {
		misc_free_file_buffer(&file);
		return -1;
	}
	line = 1;
	while ((size_t) file.pos < file.length) {
		skip_blanks(&file);
		current = misc_get_char(&file, 0);
		switch (current) {
		case '[':
			file.pos++;
			rc = scan_section_heading(&file, &array[pos++], line);
			break;
		case ':':
			file.pos++;
			rc = scan_menu_heading(&file, &array[pos++], line);
			break;
		case '#':
			file.pos++;
			skip_line(&file);
			rc = 0;
			break;
		case '\n':
			file.pos++;
			rc = 0;
			break;
		case EOF:
			rc = 0;
			break;
		default:
			if (search_line_for(&file, '=')) {
				if (isdigit(current))
					rc = scan_number_assignment(
						&file, &array[pos++], line);
				else
					rc = scan_keyword_assignment(
						&file, &array[pos++], line);
			} else {
				error_reason("Line %d: unrecognized directive",
					     line);
				rc = -1;
			}
		}
		if (rc)
			break;
		line++;
		/* Enlarge array if there is only one position left */
		if (pos + 1 >= size) {
			size *= 2;
			buffer = (struct scan_token *)
					misc_malloc(size *
						    sizeof(struct scan_token));
			if (buffer == NULL) {
				rc = -1;
				break;
			}
			memset(buffer, 0, size);
			memcpy(buffer, array, pos*sizeof(struct scan_token));
			free(array);
			array = buffer;
		}
	}
	misc_free_file_buffer(&file);
	if (rc)
		scan_free(array);
	else
		*token = array;
	return rc;
}


/* Search scanned tokens SCAN for a section/menu heading (according to
 * TYPE) of the given NAME, beginning at token OFFSET. Return the index of
 * the section/menu heading on success, a negative value on error. */
int
scan_find_section(struct scan_token* scan, char* name, enum scan_id type,
		  int offset)
{
	int i;

	for (i=offset; (int) scan[i].id != 0; i++) {
		if ((scan[i].id == scan_id_section_heading) &&
		    (type == scan_id_section_heading))
			if (strcmp(scan[i].content.section.name, name) == 0)
				return i;
		if ((scan[i].id == scan_id_menu_heading) &&
		    (type == scan_id_menu_heading))
			if (strcmp(scan[i].content.menu.name, name) == 0)
				return i;
	}
	return -1;
}


/* Check whether a string contains a load address as comma separated hex
 * value at end of string. */
static int
contains_address(char* string)
{
	unsigned long long result;

	/* Find trailing comma */
	string = strrchr(string, ',');
	if (string != NULL) {
		/* Try to scan a hexadecimal address */
		if (sscanf(string + 1, "%llx", &result) == 1) {
			return 1;
		}
	}
	return 0;
}


enum scan_section_type
scan_get_section_type(char* keyword[])
{
	if (keyword[(int) scan_keyword_tape] != NULL)
		return section_ipl_tape;
	else if (keyword[(int) scan_keyword_image] != NULL)
		return section_ipl;
	else if (keyword[(int) scan_keyword_segment] != NULL)
		return section_segment;
	else if (keyword[(int) scan_keyword_dumpto] != NULL)
		return section_dump;
	else if (keyword[(int) scan_keyword_dumptofs] != NULL)
		return section_dumpfs;
	else if (keyword[(int) scan_keyword_mvdump] != NULL)
		return section_mvdump;
	else
		return section_invalid;
}

enum scan_target_type
scan_get_target_type(char *type)
{
	if (strcasecmp(type, "SCSI") == 0)
		return target_type_scsi;
	else if (strcasecmp(type, "FBA") == 0)
		return target_type_fba;
	else if (strcasecmp(type, "LDL") == 0)
		return target_type_ldl;
	else if (strcasecmp(type, "CDL") == 0)
		return target_type_cdl;
	return target_type_invalid;
}

#define MAX(a,b)	((a)>(b)?(a):(b))

/* Check section data for correctness. KEYWORD[keyword_id] defines whether
 * a keyword is present, LINE[keyword_id] specifies in which line a keyword
 * was found or NULL when specified on command line, NAME specifies the
 * section name or NULL when specified on command line. */
int
scan_check_section_data(char* keyword[], int* line, char* name,
			int section_line, enum scan_section_type* type)
{
	char* main_keyword;
	int i;

	main_keyword = "";
	/* Find out what type this section is */
	if (*type == section_invalid) {
		if (keyword[(int) scan_keyword_tape]) {
			*type = section_ipl_tape;
			main_keyword = scan_keyword_name(scan_keyword_tape);
		} else if (keyword[(int) scan_keyword_image]) {
			*type = section_ipl;
			main_keyword = scan_keyword_name(scan_keyword_image);
		} else if (keyword[(int) scan_keyword_segment]) {
			*type = section_segment;
			main_keyword = scan_keyword_name(scan_keyword_segment);
		} else if (keyword[(int) scan_keyword_dumpto]) {
			*type = section_dump;
			main_keyword = scan_keyword_name(scan_keyword_dumpto);
		} else if (keyword[(int) scan_keyword_dumptofs]) {
			*type = section_dumpfs;
			main_keyword = scan_keyword_name(scan_keyword_dumptofs);
		} else if (keyword[(int) scan_keyword_mvdump]) {
			*type = section_mvdump;
			main_keyword = scan_keyword_name(scan_keyword_mvdump);
		} else {
			error_reason("Line %d: section '%s' must contain "
				     "either one of keywords 'image', "
				     "'segment', 'dumpto', 'dumptofs', "
				     "'mvdump' or 'tape'", section_line, name);
			return -1;
		}
	}
	/* Check keywords */
	for (i=0; i < SCAN_KEYWORD_NUM; i++) {
		switch (scan_key_table[(int) *type][i]) {
		case req:
			/* Check for missing data */
			if ((keyword[i] == 0) && (line != NULL)) {
				/* Missing keyword in config file section */
				error_reason("Line %d: missing keyword '%s' "
					     "in section '%s'", section_line,
					     scan_keyword_name(
						     (enum scan_keyword_id) i),
					     name);
				return -1;
			} else if ((keyword[i] == 0) && (line == NULL)) {
				/* Missing keyword on command line */
				error_reason("Option '%s' required when "
					     "specifying '%s'",
					     scan_keyword_name(
						     (enum scan_keyword_id) i),
					     main_keyword);

				return -1;
			}
			break;
		case inv:
			/* Check for invalid data */
			if ((keyword[i] != 0) && (line != NULL)) {
				/* Invalid keyword in config file section */
				error_reason("Line %d: keyword '%s' not "
					     "allowed in section '%s'",
					     line[i],
					     scan_keyword_name(
						     (enum scan_keyword_id) i),
					     name);
				return -1;
			} else if ((keyword[i] != 0) && (line == NULL)) {
				/* Invalid keyword on command line */
				error_reason("Only one of options '%s' and "
					     "'%s' allowed", main_keyword,
					     scan_keyword_name(
						     (enum scan_keyword_id) i));
				return -1;
			}
			break;
		case opt:
			break;
		}
	}
	/* Additional check needed for segment */
	i = (int) scan_keyword_segment;
	if (keyword[i] != NULL) {
		if (!contains_address(keyword[i])) {
			if (line != NULL) {
				error_reason("Line %d: keyword 'segment' "
					     "requires "
					     "load address", line[i]);
			} else {
				error_reason("Option 'segment' requires "
					     "load address");
			}
			return -1;
		}
	}
	/* Additional check needed for defaultboot */
	if (*type == section_defaultboot) {
		if ((keyword[(int) scan_keyword_default] != 0) &&
		    (keyword[(int) scan_keyword_defaultmenu] != 0)) {
			error_reason("Line %d: Only one of keywords 'default' "
				     "and 'defaultmenu' allowed in section "
				     "'defaultboot'",
				     MAX(line[(int) scan_keyword_default],
					 line[(int) scan_keyword_defaultmenu]));
			return -1;
		}
		if ((keyword[(int) scan_keyword_default] == 0) &&
		    (keyword[(int) scan_keyword_defaultmenu] == 0)) {
			error_reason("Line %d: Section 'defaultboot' requires "
				     "one of keywords 'default' and "
				     "'defaultmenu'", section_line);
			return -1;
		}
		
	}
	return 0;
}


static int
check_blocksize(int size)
{
	switch (size) {
	case 512:
	case 1024:
	case 2048:
	case 4096:
		return 0;
	}
	return -1;
}


int
scan_check_target_data(char* keyword[], int* line)
{
	int cylinders, heads, sectors;
	char dummy;
	int number;
	enum scan_keyword_id errid;

	if (keyword[(int) scan_keyword_targetbase] == 0) {
		if (keyword[(int) scan_keyword_targettype] != 0)
			errid = scan_keyword_targettype;
		else if ((keyword[(int) scan_keyword_targetgeometry] != 0))
			errid = scan_keyword_targetgeometry;
		else if ((keyword[(int) scan_keyword_targetblocksize] != 0))
			errid = scan_keyword_targetblocksize;
		else if ((keyword[(int) scan_keyword_targetoffset] != 0))
			errid = scan_keyword_targetoffset;
		else
			return 0;
		if (line != NULL)
			error_reason("Line %d: keyword 'targetbase' required "
				"when specifying '%s'",
				line[(int) errid], scan_keyword_name(errid));
		else
			error_reason("Option 'targetbase' required when "
				"specifying '%s'",
				scan_keyword_name(errid));
		return -1;
	}
	if (keyword[(int) scan_keyword_targettype] == 0) {
		if (line != NULL)
			error_reason("Line %d: keyword 'targettype' "
				"required when specifying 'targetbase'",
				line[(int) scan_keyword_targetbase]);
		else
			error_reason("Option 'targettype' required "
				     "when specifying 'targetbase'");
		return -1;
	}
	switch (scan_get_target_type(keyword[(int) scan_keyword_targettype])) {
	case target_type_cdl:
	case target_type_ldl:
		if ((keyword[(int) scan_keyword_targetgeometry] != 0))
			break;
		if (line != NULL)
			error_reason("Line %d: keyword 'targetgeometry' "
				"required when specifying 'targettype' %s",
				line[(int) scan_keyword_targettype],
				keyword[(int) scan_keyword_targettype]);
		else
			error_reason("Option 'targetgeometry' required when "
				"specifying 'targettype' %s",
				keyword[(int) scan_keyword_targettype]);
		return -1;
	case target_type_scsi:
	case target_type_fba:
		if ((keyword[(int) scan_keyword_targetgeometry] == 0))
			break;
		if (line != NULL)
			error_reason("Line %d: keyword "
				"'targetgeometry' not allowed for "
				"'targettype' %s",
				line[(int) scan_keyword_targetgeometry],
				keyword[(int) scan_keyword_targettype]);
		else
			error_reason("Keyword 'targetgeometry' not "
				"allowed for 'targettype' %s",
				keyword[(int) scan_keyword_targettype]);
		return -1;
	case target_type_invalid:
		if (line != NULL)
			error_reason("Line %d: Unrecognized 'targettype' value "
				"'%s'",
				line[(int) scan_keyword_targettype],
				keyword[(int) scan_keyword_targettype]);
		else
			error_reason("Unrecognized 'targettype' value '%s'",
				keyword[(int) scan_keyword_targettype]);
		return -1;
	}
	if (keyword[(int) scan_keyword_targetgeometry] != 0) {
		if ((sscanf(keyword[(int) scan_keyword_targetgeometry],
		    "%d,%d,%d %c", &cylinders, &heads, &sectors, &dummy)
		    != 3) || (cylinders <= 0) || (heads <= 0) ||
		    (sectors <= 0)) {
			if (line != NULL)
				error_reason("Line %d: Invalid target geometry "
					"'%s'", line[
					(int) scan_keyword_targetgeometry],
					keyword[
					(int) scan_keyword_targetgeometry]);
			else
				error_reason("Invalid target geometry '%s'",
					keyword[
					(int) scan_keyword_targetgeometry]);
			return -1;
		}
	}
	if (keyword[(int) scan_keyword_targetblocksize] == 0) {
		if (line != NULL)
			error_reason("Line %d: Keyword 'targetblocksize' "
				"required when specifying 'targetbase'",
				line[(int) scan_keyword_targetbase]);
		else
			error_reason("Option 'targetblocksize' required when "
				"specifying 'targetbase'");
		return -1;
	}
	if ((sscanf(keyword[(int) scan_keyword_targetblocksize], "%d %c",
	    &number, &dummy) != 1) || check_blocksize(number)) {
		if (line != NULL)
			error_reason("Line %d: Invalid target blocksize '%s'",
				line[(int) scan_keyword_targetblocksize],
				keyword[(int) scan_keyword_targetblocksize]);
		else
			error_reason("Invalid target blocksize '%s'",
				keyword[(int) scan_keyword_targetblocksize]);
		return -1;
	}
	if (keyword[(int) scan_keyword_targetoffset] == 0) {
		if (line != NULL)
			error_reason("Line %d: Keyword 'targetoffset' "
				"required when specifying 'targetbase'",
				line[(int) scan_keyword_targetbase]);
		else
			error_reason("Option 'targetoffset' required when "
				"specifying 'targetbase'");
		return -1;
	}
	if (sscanf(keyword[(int) scan_keyword_targetoffset], "%d %c",
	    &number, &dummy) != 1) {
		if (line != NULL)
			error_reason("Line %d: Invalid target offset '%s'",
				line[(int) scan_keyword_targetoffset],
				keyword[(int) scan_keyword_targetoffset]);
		else
			error_reason("Invalid target offset '%s'",
				keyword[(int) scan_keyword_targetoffset]);
		return -1;
	}
	return 0;
}

/* Check section at INDEX for compliance with config file rules. Upon success,
 * return zero and advance INDEX to point to the end of the section. Return
 * non-zero otherwise. */
static int
check_section(struct scan_token* scan, int* index)
{
	char* name;
	char* keyword[SCAN_KEYWORD_NUM];
	int keyword_line[SCAN_KEYWORD_NUM];
	enum scan_keyword_id key_id;
	enum scan_section_type type;
	int line;
	int i;
	int rc;
	
	i = *index;
	name = scan[i].content.section.name;
	/* Ensure unique section names */
	line = scan_find_section(scan, name, scan_id_section_heading, i+1);
	if (line >= 0) {
		error_reason("Line %d: section name '%s' already specified",
			     scan[line].line, name);
		return -1;
	}
	/* Check for defaultboot section */
	if (strcmp(name, "defaultboot") == 0)
		type = section_defaultboot;
	else
		type = section_invalid;
	memset(keyword, 0, sizeof(keyword));
	memset(keyword_line, 0, sizeof(keyword_line));
	line = scan[i].line;
	/* Account for keywords */
	for (i++; (int) scan[i].id != 0; i++) {
		if ((scan[i].id == scan_id_section_heading) ||
		    (scan[i].id == scan_id_menu_heading))
			break;
		else if (scan[i].id == scan_id_keyword_assignment) {
			key_id = scan[i].content.keyword.keyword;
			keyword_line[key_id] = scan[i].line;
			if (keyword[(int) key_id] != NULL) {
				/* Rule 5 */
				error_reason("Line %d: keyword '%s' already "
					     "specified", scan[i].line,
					     scan_keyword_name(key_id));
				return -1;
			}
			keyword[(int) key_id] = scan[i].content.keyword.value;
			/* Check default keyword value */
			if (key_id == scan_keyword_default) {
				/* Rule 8 */
				if (scan_find_section(scan,
				     scan[i].content.keyword.value,
				     scan_id_section_heading, 0) < 0) {
					error_reason("Line %d: no such section "
						"'%s'", scan[i].line,
						scan[i].content.keyword.value);
					return -1;
				}
			}
		} else if (scan[i].id == scan_id_number_assignment) {
			/* Rule 2 */
			error_reason("Line %d: number assignment not allowed "
				     "in section '%s'", scan[i].line,
				     name);
			return -1;
		}
	}
	rc = scan_check_section_data(keyword, keyword_line, name, line, &type);
	if (rc)
		return rc;
	/* Check target data */
	rc = scan_check_target_data(keyword, keyword_line);
	if (rc)
		return rc;
	/* Advance index to end of section */
	*index = i - 1;
	return 0;
}


static int
find_num_assignment(struct scan_token* scan, int num, int offset)
{
	int i;

	for (i=offset; (int) scan[i].id != 0; i++) {
		if (scan[i].id == scan_id_number_assignment) {
			if (scan[i].content.number.number == num)
				return i;
		} else if ((scan[i].id == scan_id_section_heading) ||
			   (scan[i].id == scan_id_menu_heading))
			break;
	}
	return -1;
}


/* Check menu section at INDEX for compliance with config file rules. Upon
 * success, return zero and advance INDEX to point to the end of the section.
 * Return non-zero otherwise. */
static int
check_menu(struct scan_token* scan, int* index)
{
	char* keyword[SCAN_KEYWORD_NUM];
	int keyword_line[SCAN_KEYWORD_NUM];
	enum scan_keyword_id key_id;
	char* name;
	char* str;
	long pos;
	int line;
	int i;
	int j;
	int is_num;
	int is_target;
	int is_default;
	int is_prompt;
	int is_timeout;
	int rc;

	i = *index;
	name = scan[i].content.menu.name;
	/* Rule 15 */
	line = scan_find_section(scan, name, scan_id_menu_heading, i+1);
	if (line >= 0) {
		error_reason("Line %d: menu name '%s' already specified",
			     scan[line].line, name);
		return -1;
	}
	memset(keyword, 0, sizeof(keyword));
	memset(keyword_line, 0, sizeof(keyword_line));
	line = scan[i].line;
	is_num = 0;
	is_target = 0;
	is_default = 0;
	is_prompt = 0;
	is_timeout = 0;
	/* Account for keywords */
	for (i++; (int) scan[i].id != 0; i++) {
		if ((scan[i].id == scan_id_section_heading) ||
		    (scan[i].id == scan_id_menu_heading))
			break;
		else if (scan[i].id == scan_id_keyword_assignment) {
			key_id = scan[i].content.keyword.keyword;
			switch (key_id) {
			case scan_keyword_target:
				if (is_target) {
					error_reason("Line %d: keyword '%s' "
						     "already specified",
						     scan[i].line,
						     scan_keyword_name(key_id));
					return -1;
				}
				is_target = 1;
				break;
			case scan_keyword_targetbase:
			case scan_keyword_targettype:
			case scan_keyword_targetgeometry:
			case scan_keyword_targetblocksize:
			case scan_keyword_targetoffset:
				key_id = scan[i].content.keyword.keyword;
				keyword_line[key_id] = scan[i].line;
				/* Rule 5 */
				if (keyword[(int) key_id] != NULL) {
					error_reason("Line %d: keyword '%s' "
						"already specified",
						scan[i].line,
						scan_keyword_name(key_id));
					return -1;
				}
				keyword[(int) key_id] =
					scan[i].content.keyword.value;
				break;
			case scan_keyword_default:
				if (is_default) {
					error_reason("Line %d: keyword '%s' "
						     "already specified",
						     scan[i].line,
						     scan_keyword_name(key_id));
					return -1;
				}
				is_default = 1;
				/* Rule 16 */
				str = scan[i].content.keyword.value;
				/* Skip leading blanks */
				for (; isspace(*str); str++) ;
				if (*str == 0) {
					error_reason("Line %d: default "
						     "position must be a "
						     "number", scan[i].line);
					return -1;
				}
				pos = strtol(str, &str, 10);
				/* Skip blanks after number */
				for (; isspace(*str); str++) ;
				if (*str != 0) {
					error_reason("Line %d: default "
						     "position must be a "
						     "number", scan[i].line);
					return -1;
				}
				if (pos <= 0) {
					error_reason("Line %d: default "
						     "position must be greater "
						     "than zero",
						     scan[i].line);
					return -1;
				}
				if ((pos == LONG_MAX) && (errno == ERANGE)) {
					error_reason("Line %d: default "
						     "position too large",
						     scan[i].line);
					return -1;
				}
				j = find_num_assignment(scan, pos, *index+1);
				if (j < 0) {
					error_reason("Line %d: menu "
						     "position %ld not "
						     "defined",
						     scan[i].line, pos);
					return -1;
				}
				break;
			case scan_keyword_prompt:
				if (is_prompt) {
					error_reason("Line %d: keyword '%s' "
						     "already specified",
						     scan[i].line,
						     scan_keyword_name(key_id));
					return -1;
				}
				is_prompt = 1;
				str = scan[i].content.keyword.value;
				/* Skip leading blanks */
				for (; isspace(*str); str++) ;
				if (*str == 0) {
					error_reason("Line %d: prompt value "
						     "must be a number",
						     scan[i].line);
					return -1;
				}
				pos = strtol(str, &str, 10);
				/* Skip blanks after number */
				for (; isspace(*str); str++) ;
				if (*str != 0) {
					error_reason("Line %d: prompt value "
						     "must be a number",
						     scan[i].line);
					return -1;
				}
				break;
			case scan_keyword_timeout:
				if (is_timeout) {
					error_reason("Line %d: keyword '%s' "
						     "already specified",
						     scan[i].line,
						     scan_keyword_name(key_id));
					return -1;
				}
				is_timeout = 1;
				str = scan[i].content.keyword.value;
				/* Skip leading blanks */
				for (; isspace(*str); str++) ;
				if (*str == 0) {
					error_reason("Line %d: timeout value "
						     "must be a number",
						     scan[i].line);
					return -1;
				}
				pos = strtol(str, &str, 10);
				/* Skip blanks after number */
				for (; isspace(*str); str++) ;
				if (*str != 0) {
					error_reason("Line %d: timeout value "
						     "must be a number",
						     scan[i].line);
					return -1;
				}
				break;
			default:
				/* Rule 13 */
				error_reason("Line %d: keyword '%s' not "
					     "allowed in menu", scan[i].line,
					     scan_keyword_name(key_id));
				return -1;
			}
		} else if (scan[i].id == scan_id_number_assignment) {
			is_num = 1;
			if (scan[i].content.number.number < 1) {
				error_reason("Line %d: position must be "
					     "greater than zero",
					     scan[i].line);
				return -1;
			}
			/* Rule 10 */
			j = find_num_assignment(scan,
						scan[i].content.number.number,
						i+1);
			if (j >= 0) {
				error_reason("Line %d: position %d already "
					     "specified",
					     scan[j].line,
					     scan[i].content.number.number);
				return -1;
			}
			/* Rule 11 */
			j = scan_find_section(scan,
					      scan[i].content.number.value,
					      scan_id_section_heading, 0);
			if (j < 0) {
				error_reason("Line %d: section '%s' not found",
					     scan[i].line,
					     scan[i].content.number.value);
				return -1;
			}

		}
	}
	/* Rule 9 */
	if (!is_num) {
		error_reason("Line %d: no boot configuration specified in "
			     "menu '%s'", line, name);
		return -1;
	}
	/* Rule 12 */
	if (!is_target) {
		error_reason("Line %d: missing keyword '%s' in menu '%s'",
			     line, scan_keyword_name(scan_keyword_target),
			     name);
		return -1;
	}
	/* Check target data */
	rc = scan_check_target_data(keyword, keyword_line);
	if (rc)
		return rc;
	/* Advance index to end of menu section */
	*index = i - 1;
	return 0;
}


/* Check scanned tokens for compliance with config file rules. Return zero
 * if config file complies, non-zero otherwise. Rules are:
 *
 * Global:
 *  1 - no keywords are allowed outside of a section
 *
 * Configuration sections:
 *  2 - may not contain number assignments
 *  3 - must contain certain keywords according to type (see keyword_table)
 *  4 - may not contain certain keywords according to type (see keyword_table)
 *  5 - keywords may be specified at most once
 *  6 - must contain at least one keyword assignment
 *  7 - section name must be unique in the configuration file
 *  8 - defaultboot:default must point to a valid section
 *
 * Menu sections:
 *  9  - must contain at least one number assignment
 *  10 - numbers in number assignment have to be unique in the section
 *  11 - referenced sections must be present
 *  12 - must contain certain keywords according to type (see keyword_table)
 *  13 - may not contain certain keywords according to type (see keyword_table)
 *  14 - keywords may be specified at most once
 *  15 - menu name must be unique in the configuration file
 *  16 - optional default position must be a valid position
 * */
int
scan_check(struct scan_token* scan)
{
	int in_section;
	int i;
	int rc;

	in_section = 0;
	for (i=0; (int) scan[i].id != 0; i++) {
		switch (scan[i].id) {
		case scan_id_section_heading:
			in_section = 1;
			rc = check_section(scan, &i);
			if (rc)
				return rc;
			break;
		case scan_id_menu_heading:
			in_section = 1;
			rc = check_menu(scan, &i);
			if (rc)
				return rc;
			break;
		case scan_id_keyword_assignment:
			/* Rule 1 */
			if (!in_section) {
				error_reason("Line %d: keyword assignment not "
					     "allowed outside of section",
					     scan[i].line);
				return -1;
			}
			break;
		case scan_id_number_assignment:
			/* Rule 1 */
			if (!in_section) {
				error_reason("Line %d: number assignment not "
					     "allowed outside of section",
					     scan[i].line);
				return -1;
			}
			break;
		default:
			break;
		}
	}
	return 0;
}
