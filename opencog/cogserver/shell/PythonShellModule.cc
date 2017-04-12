/*
 * PythonShellModule.cpp
 *
 * Module for starting up python shell
 *
 * @author Ramin Barati <rekino@gmail.com>
 * @date   2013-07-02
 *
 * @Note
 *   This code is almost identical to Linas' SchemeShellModule, so most of the credits
 *   goes to him.
 *
 * Reference:
 *   http://www.linuxjournal.com/article/3641?page=0,2
 *   http://www.codeproject.com/KB/cpp/embedpython_1.aspx
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifdef HAVE_CYTHON

#include <opencog/util/Logger.h>
#include <opencog/util/oc_assert.h>
#include <opencog/util/platform.h>

#include <opencog/cython/PythonEval.h>
#include <opencog/cogserver/server/ConsoleSocket.h>
#include "PythonShellModule.h"

namespace opencog
{

DECLARE_MODULE(PythonShellModule);

PythonShellModule::PythonShellModule(CogServer& cs) : Module(cs)
{
}

PythonShellModule::~PythonShellModule()
{
    shellout_unregister();
    do_eval_unregister();
}

void PythonShellModule::init(void)
{
    shellout_register();
    do_eval_register();
}

std::string PythonShellModule::shellout(Request *req, std::list<std::string> args)
{
    ConsoleSocket *con = req->get_console();
    OC_ASSERT(con, "Invalid Request object");

    PythonShell *sh = new PythonShell();
    sh->set_socket(con);

    bool hush = false;
    if (!args.empty())
    {
        std::string &arg = args.front();
        if (arg.compare("quiet") || arg.compare("hush")) hush = true;
    }
    sh->hush_prompt(hush);

    if (hush) return "";

    std::string rv =
        "Entering python shell; use ^D or a single . on a "
        "line by itself to exit.\n" + sh->get_prompt();
    return rv;
}

std::string PythonShellModule::do_eval(Request *req, std::list<std::string> args)
{
    // Needs to join the args back up into one string.
    std::string expr;
    std::string out = "test";

    // Adds an extra space on the end, but that doesn't matter.
    for (std::string arg : args)
    {
        expr += arg + " ";
    }

    PythonEval& eval = PythonEval::instance();
    eval.begin_eval();
    eval.eval_expr(expr);
    out = eval.poll_result();
    // May not be necessary since an error message and backtrace are provided.
//	if (eval.eval_error()) {
//		out += "An error occurred\n";
//	}
    if (eval.input_pending()) {
        out += "Invalid Python expression: missing something";
    }
    eval.clear_pending();

    return out;
}

}
 #endif
