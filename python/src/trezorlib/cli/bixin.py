# This file is part of the Trezor project.
#
# Copyright (C) 2012-2019 SatoshiLabs and contributors
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3
# as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the License along with this library.
# If not, see <https://www.gnu.org/licenses/lgpl-3.0.html>.


import click

from .. import debuglink, device
from . import with_client


@click.group(name="bixin")
def cli():
    """Device management commands - setup, recover seed, wipe, etc."""


@cli.command()
@with_client
def self_test(client):
    """Perform a self-test."""
    return debuglink.self_test(client)


@cli.command()
@with_client
def backup(client):
    """Perform device seed backup."""
    ret = device.se_backup(client)
    return "data: {}".format(ret.hex())


@cli.command()
@click.argument("hex_data")
@with_client
def restore(client, hex_data):
    """Perform device seed restore."""
    data = bytes.fromhex(hex_data)
    return device.se_restore(client, data)


@cli.command()
@click.argument("hex_data")
@with_client
def verify(client, hex_data):
    """Perform device verify."""
    data = bytes.fromhex(hex_data)
    ret = device.se_verify(client, data)
    return "data: {}".format(ret.hex())


@cli.command()
@click.option("-p", "--no-pin", type=bool, help="no pin")
@click.option("-c", "--no-confirm", type=bool, help="no confirm")
@click.option("-l", "--pay-limit", type=int)
@click.option("-t", "--pay-times", type=int, default=10, help="pay times")
@with_client
def free(client, no_pin, no_confirm, pay_limit, pay_times):
    """set pay no pin or no confirm"""
    return device.apply_settings(
        client,
        fee_pay_pin=no_pin,
        fee_pay_confirm=no_confirm,
        fee_pay_money_limit=pay_limit,
        fee_pay_times=pay_times,
    )
