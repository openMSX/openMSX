// $Id$

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include "CliCommInput.hh"
#include "CommandController.hh"
#include "Scheduler.hh"

using std::cout;

namespace openmsx {

static xmlSAXHandler sax_handler;
static xmlParserCtxt* parser_context;
CliCommInput::ParseState CliCommInput::user_data;


CliCommInput::CliCommInput()
	: lock(1), thread(this)
{
	thread.start();
}

CliCommInput::~CliCommInput()
{
}

void CliCommInput::cb_start_element(ParseState* user_data,
                     const xmlChar* name, const xmlChar** attrs)
{
	switch (user_data->state) {
		case START:
			if (strcmp((const char*)name, "openmsx-control") == 0) {
				user_data->state = TAG_OPENMSX;
			}
			break;
		case TAG_OPENMSX:
			if (strcmp((const char*)name, "command") == 0) {
				user_data->state = TAG_COMMAND;
			}
			break;
		default:
			break;
	}
	user_data->content = ""; // clear() doesn't compile on gcc-2.95
}

void CliCommInput::cb_end_element(ParseState* user_data, const xmlChar* name)
{
	switch (user_data->state) {
		case TAG_OPENMSX:
			user_data->state = START;
			break;
		case TAG_COMMAND:
			user_data->object->execute(user_data->content);
			user_data->state = TAG_OPENMSX;
			break;
		default:
			break;
	}
}

void CliCommInput::cb_text(ParseState* user_data, const xmlChar* chars, int len)
{
	if (user_data->state == TAG_COMMAND) {
		user_data->content.append((const char*)chars, len);
	}
}

void CliCommInput::run() throw()
{
	user_data.state = START;
	user_data.object = this;
	memset(&sax_handler, 0, sizeof(sax_handler));
	sax_handler.startElement = (startElementSAXFunc)cb_start_element;
	sax_handler.endElement   = (endElementSAXFunc)  cb_end_element;
	sax_handler.characters   = (charactersSAXFunc)  cb_text;
	
	parser_context = xmlCreatePushParserCtxt(&sax_handler, &user_data, 0, 0, 0);
	
	char buf[4096];
	while (true) {
		ssize_t n = read(STDIN_FILENO, buf, 4096);
		if (n == -1) {
			break;
		}
		xmlParseChunk(parser_context, buf, n, 0);
	}
	xmlFreeParserCtxt(parser_context);
}

void CliCommInput::execute(const string& command)
{
	lock.down();
	cmds.push_back(command);
	lock.up();
	Scheduler::instance().setSyncPoint(Scheduler::ASAP, this);
}

void CliCommInput::executeUntil(const EmuTime& time, int userData) throw()
{
	CommandController& controller = CommandController::instance();
	lock.down();
	while (!cmds.empty()) {
		try {
			string result = controller.executeCommand(cmds.front());
			cout << "<ok>" << result << "</ok>" << endl;
		} catch (CommandException &e) {
			string result = e.getMessage() + '\n';
			cout << "<nok>" << result << "</nok>" << endl;
		}
		cmds.pop_front();
	}
	lock.up();
}

const string& CliCommInput::schedName() const
{
	static const string NAME("CliCommInput");
	return NAME;
}

} // namespace openmsx
