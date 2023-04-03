/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "info.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
// TODO@snapshots move this to common_cli once required by other commands
std::vector<mp::InstanceSnapshotPair> add_instance_and_snapshot_names(const mp::ArgParser* parser)
{
    std::vector<mp::InstanceSnapshotPair> instance_snapshot_names;
    instance_snapshot_names.reserve(parser->positionalArguments().count());

    for (const auto& arg : parser->positionalArguments())
    {
        mp::InstanceSnapshotPair inst_snap_name;
        auto index = arg.indexOf('.');
        inst_snap_name.set_instance_name(arg.left(index).toStdString());
        if (index >= 0)
            inst_snap_name.set_snapshot_name(arg.right(arg.length() - index - 1).toStdString());

        instance_snapshot_names.push_back(inst_snap_name);
    }

    return instance_snapshot_names;
}
} // namespace

mp::ReturnCode cmd::Info::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::InfoReply& reply) {
        cout << chosen_formatter->format(reply);

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::info, request, on_success, on_failure);
}

std::string cmd::Info::name() const { return "info"; }

QString cmd::Info::short_help() const
{
    return QStringLiteral("Display information about instances");
}

QString cmd::Info::description() const
{
    return QStringLiteral("Display information about instances");
}

mp::ParseCode cmd::Info::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to display information about", "<name> [<name> ...]");

    QCommandLineOption all_option(all_option_name, "Display info for all instances");
    QCommandLineOption noRuntimeInfoOption(
        "no-runtime-information",
        "Retrieve from the daemon only the information obtained without running commands on the instance");
    noRuntimeInfoOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption formatOption(
        "format", "Output info in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");
    QCommandLineOption snapshotOverviewOption("snapshot-overview", "Display info on snapshots");

    parser->addOptions({all_option, noRuntimeInfoOption, formatOption, snapshotOverviewOption});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr);
    if (parse_code != ParseCode::Ok)
        return parse_code;

    for (const auto& item : add_instance_and_snapshot_names(parser))
        request.add_instances_snapshots()->CopyFrom(item);
    request.set_no_runtime_information(parser->isSet(noRuntimeInfoOption));
    request.set_snapshot_overview(parser->isSet(snapshotOverviewOption));

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}
