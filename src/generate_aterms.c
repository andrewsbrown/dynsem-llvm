#include "generate.h"
#include "generate_aterms.h"
#include <aterm1.h>
#include <aterm2.h>
#include "log.h"

void generate_headers(FILE *stream) {
    fputs("#include <aterm1.h>\n", stream);
    fputs("#include <aterm2.h>\n", stream);
    fputs("#include \"transform.h\"\n", stream);
    fputs("\n", stream);
    fflush(stream);
}

void generate_rule_table(FILE *stream, RuleTable *rules) {
    fprintf(stream, "Rule rule_table[%d];\n", rules->length);
    fputs("void build_rules() {\n", stream);

    for (int i = 0; i < rules->length; i++) {
        generate_rule_table_entry(stream, rules->rules[i]);
    }

    fputs("}\n", stream);
    fflush(stream);
}

void generate_rule_table_entry(FILE *stream, Rule rule) {
    fprintf(stream, "\t rule_table[%d].from = ATmake(\"", rule.id);
    ATfprintf(stream, "%t", replace_free_variables(rule.from));
    fputs("\");\n", stream);

    fprintf(stream, "\t rule_table[%d].to = ATmake(\"", rule.id);
    ATfprintf(stream, "%t", replace_free_variables(rule.to));
    fputs("\");\n", stream);
}

void generate_find_function(FILE *stream, RuleTable *rules) {
    fputs("ATerm match_and_transform(ATerm before) {\n", stream);

    for (int i = 0; i < rules->length; i++) {
        fputs("\t ", stream);
        generate_find_case(stream, rules->rules[i]);
        fputs("\n", stream);
    }

    fputs("\t return NULL;\n", stream);
    fputs("}\n", stream);
    fflush(stream);
}

void generate_find_case(FILE *stream, Rule rule) {
    fprintf(stream, "if(ATmatchTerm(rule_table[%d].from, before)) return transform_%d(rule_table[%d].from, rule_table[%d].to, before);", rule.id, rule.id, rule.id, rule.id);
    fflush(stream);
}

void generate_transform_functions(FILE *stream, RuleTable *rules) {
    fputs("\n", stream);

    for (int i = 0; i < rules->length; i++) {
        generate_transform_function(stream, rules->rules[i]);
        fputs("\n", stream);
    }

    fflush(stream);
}

void generate_transform_function(FILE *stream, Rule rule) {
    fprintf(stream, "ATerm transform_%d(ATerm from, ATerm to, ATerm before) {\n", rule.id);

    // allocate from variables: ATerm a, b, c;
    ATermList from_vars = find_free_variables(rule.from, ATempty);
    generate_variable_list(stream, from_vars, "\t ATerm ", ", ", ";\n");

    // fill in values: ATmatchTerm(before, from, &a, &b, &c);
    // TODO need error check?
    fputs("\t ATmatchTerm(before, from", stream);
    generate_variable_list(stream, from_vars, ", &", ", &", NULL); // TODO need prefix
    fputs(");\n", stream);

    // make after term from to rule: return ATmakeTerm(to, a, c);
    ATermList to_vars = find_free_variables(rule.to, ATempty);
    fputs("\t return ATmakeTerm(to", stream);
    generate_variable_list(stream, to_vars, ", ", ", ", NULL);
    fputs(");\n", stream);

    fputs("}\n", stream);
    fflush(stream);
}

ATermList find_free_variables(ATerm term, ATermList collector) {
    switch (ATgetType(term)) {
        case AT_APPL:
            trace("appl");
            AFun fun = ATgetAFun((ATermAppl) term);
            int arity = ATgetArity(fun);
            if (arity == 0) {
                return ATinsert(collector, term);
            } else {
                for (int i = 0; i < arity; i++) {
                    collector = find_free_variables(ATgetArgument((ATermAppl) term, i), collector);
                }
            }
            break;
        case AT_LIST:
            trace("list");
            int length = ATgetLength((ATermList) term);
            for (int i = 0; i < length; i++) {
                collector = find_free_variables(ATelementAt((ATermList) term, i), collector);
            }
            break;
        default:
            // only lists and applications can contain 0-arity functions
            break;
    }
    return ATreverse(collector);
}

void generate_variable_list(FILE *stream, ATermList vars, char *prefix, char *delimiter, char *suffix) {
    char *name;
    ATbool vars_exist = !ATisEmpty(vars);
    if (vars_exist && prefix != NULL) fputs(prefix, stream);
    while (!ATisEmpty(vars)) {
        name = ATgetName(ATgetAFun(ATgetFirst(vars)));
        fputs(name, stream);
        vars = ATgetNext(vars);
        if (!ATisEmpty(vars) && delimiter != NULL) fputs(delimiter, stream);
    }
    if (vars_exist && suffix != NULL) fputs(suffix, stream);
    fflush(stream);
}

ATerm replace_free_variables(ATerm rule) {
    switch (ATgetType(rule)) {
        case AT_APPL:
            trace("appl");
            AFun fun = ATgetAFun((ATermAppl) rule);
            int arity = ATgetArity(fun);
            if (arity == 0) {
                // generate and return "<term>"
                AFun term = ATmakeAFun("term", 0, ATfalse);
                ATermPlaceholder term_placeholder = ATmakePlaceholder((ATerm) ATmakeAppl0(term));
                return (ATerm) term_placeholder;
            } else {
                for (int i = 0; i < arity; i++) {
                    ATerm arg = ATgetArgument(rule, i);
                    ATerm new = replace_free_variables(arg);
                    if (arg != new) rule = (ATerm) ATsetArgument((ATermAppl) rule, new, i);
                }
            }
            break;
        case AT_LIST:
            trace("list");
            int length = ATgetLength((ATermList) rule);
            for (int i = 0; i < length; i++) {
                ATerm arg = ATelementAt((ATermList) rule, i);
                ATerm new = replace_free_variables(arg);
                if (arg != new) rule = (ATerm) ATreplace((ATermList) rule, new, i);
            }
            break;
        case AT_INT:
        case AT_REAL:
        case AT_BLOB:
        case AT_SYMBOL:
            return rule;
            break;
        case AT_PLACEHOLDER:
            ATwarning("Found placeholder in rule, continuing: %t", rule); // TODO use logger
            return rule;
            break;
        case AT_FREE:
        default:
            ATerror("Found unknown or unhandleable type: %t", rule); // TODO use logger
            exit(1);
            break;
    }
    return rule;
}