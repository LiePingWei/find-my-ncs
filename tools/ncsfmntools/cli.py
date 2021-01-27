import sys
from os import path, listdir

import click

scripts_folder = path.abspath(path.join(path.dirname(__file__), 'scripts'))


class FMNToolsCLI(click.MultiCommand):

    def list_commands(self, ctx):
        rv = []
        for filename in listdir(scripts_folder):
            if filename.endswith('.py') and filename.startswith('cmd_'):
                rv.append(filename[4:-3])
        rv.sort()
        print(rv)
        return rv

    def get_command(self, ctx, name):
        try:
            if sys.version_info[0] == 2:
                name = name.encode('ascii', 'replace')
            mod = __import__(name='ncsfmntools.scripts.cmd_' + name, fromlist=['cli'])
        except ImportError:
            return
        return mod.cli


@click.command(cls=FMNToolsCLI)
def cli():
    """This is a collection of Find My Network tools."""
    pass


if __name__ == '__main__':
    cli()
