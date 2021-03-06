import logging

from cliff import command
from cliff import lister
from cliff import show

from oio.cli.utils import KeyValueAction


class ListReference(lister.Lister):
    """List reference"""

    log = logging.getLogger(__name__ + '.ListReference')

    def get_parser(self, prog_name):
        parser = super(ListReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to list'
        )
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        data = self.app.client_manager.directory.get(
            self.app.client_manager.get_account(),
            reference=parsed_args.reference
        )
        columns = ('Type', 'Host', 'Args', 'Seq')
        results = ((d['type'], d['host'], d['args'], d['seq'])
                   for d in data['srv'])
        return columns, results


class ShowReference(show.ShowOne):
    """Show reference"""

    log = logging.getLogger(__name__ + '.ShowReference')

    def get_parser(self, prog_name):
        parser = super(ShowReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to show')
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        account = self.app.client_manager.get_account()
        reference = parsed_args.reference

        data = self.app.client_manager.directory.get_properties(
            account,
            parsed_args.reference)
        info = {'account': account,
                'name': reference}
        for k, v in data.iteritems():
            info['meta.' + k] = v
        return zip(*sorted(info.iteritems()))


class CreateReference(command.Command):
    """Create reference"""

    log = logging.getLogger(__name__ + '.CreateReference')

    def get_parser(self, prog_name):
        parser = super(CreateReference, self).get_parser(prog_name)
        parser.add_argument(
            'references',
            metavar='<reference>',
            nargs='+',
            help='Reference(s) to create'
        )
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        for reference in parsed_args.references:
            self.app.client_manager.directory.create(
                self.app.client_manager.get_account(),
                reference=reference
            )


class DeleteReference(command.Command):
    """Delete reference"""

    log = logging.getLogger(__name__ + '.DeleteReference')

    def get_parser(self, prog_name):
        parser = super(DeleteReference, self).get_parser(prog_name)
        parser.add_argument(
            'references',
            metavar='<reference>',
            nargs='+',
            help='Reference(s) to delete'
        )
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        for reference in parsed_args.references:
            self.app.client_manager.directory.delete(
                self.app.client_manager.get_account(),
                reference=reference
            )


class LinkReference(command.Command):
    """Link services to reference"""

    log = logging.getLogger(__name__ + '.LinkReference')

    def get_parser(self, prog_name):
        parser = super(LinkReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to update'
        )
        parser.add_argument(
            'srv_type',
            metavar='<srv_type>',
            help='Link services of <srv_type> to reference'
        )
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        reference = parsed_args.reference
        srv_type = parsed_args.srv_type

        self.app.client_manager.directory.link(
            self.app.client_manager.get_account(),
            reference,
            srv_type
        )


class UnlinkReference(command.Command):
    """Unlink services from reference"""

    log = logging.getLogger(__name__ + '.UnlinkReference')

    def get_parser(self, prog_name):
        parser = super(UnlinkReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to unlink'
        )
        parser.add_argument(
            'srv_type',
            metavar='<srv_type>',
            help='Unlink services of <srv_type> from reference'
        )
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        reference = parsed_args.reference
        srv_type = parsed_args.srv_type

        self.app.client_manager.directory.unlink(
            self.app.client_manager.get_account(),
            reference,
            srv_type
        )


class PollReference(command.Command):
    """Poll services for reference"""

    log = logging.getLogger(__name__ + '.PollReference')

    def get_parser(self, prog_name):
        parser = super(PollReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to poll'
        )
        parser.add_argument(
            'srv_type',
            metavar='<srv_type>',
            help='Poll services of <srv_type>'
        )
        return parser

    def take_action(self, parsed_args):
        reference = parsed_args.reference
        srv_type = parsed_args.srv_type

        self.app.client_manager.directory.renew(
            self.app.client_manager.get_account(),
            reference,
            srv_type
        )


class ForceReference(command.Command):
    """Force link a service to reference"""

    log = logging.getLogger(__name__ + '.ForceReference')

    def get_parser(self, prog_name):
        parser = super(ForceReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to update'
        )
        parser.add_argument(
            'host',
            metavar='<host>',
            help='Service host'
        )
        parser.add_argument(
            'type',
            metavar='<type>',
            help="Service type"
        )
        parser.add_argument(
            '--seq',
            metavar='<seq>',
            default=1,
            type=int,
            help='Service sequence number'
        )
        parser.add_argument(
            '--args',
            metavar='<args>',
            default="",
            help='Service args'
        )
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        reference = parsed_args.reference
        service = dict(host=parsed_args.host,
                       type=parsed_args.type,
                       args=parsed_args.args,
                       seq=parsed_args.seq)

        self.app.client_manager.directory.force(
            self.app.client_manager.get_account(),
            reference,
            parsed_args.type,
            service
        )


class SetReference(command.Command):
    """Set reference properties"""

    log = logging.getLogger(__name__ + '.SetReference')

    def get_parser(self, prog_name):
        parser = super(SetReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to modify'
        )
        parser.add_argument(
            '--property',
            metavar='<key=value>',
            action=KeyValueAction,
            help='Property to add/update for this reference')
        parser.add_argument(
            '--clear',
            dest='clear',
            default=False,
            help='Clear previous properties',
            action='store_true')
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        self.app.client_manager.directory.set_properties(
            self.app.client_manager.get_account(),
            parsed_args.reference,
            parsed_args.property,
            parsed_args.clear)


class UnsetReference(command.Command):
    """Unset reference properties"""

    log = logging.getLogger(__name__ + '.UnsetReference')

    def get_parser(self, prog_name):
        parser = super(UnsetReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to modify'
        )
        parser.add_argument(
            '--property',
            metavar='<key>',
            action='append',
            default=[],
            help='Property to remove from reference',
            required=True)
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        self.app.client_manager.directory.del_properties(
            self.app.client_manager.get_account(),
            parsed_args.reference,
            parsed_args.property)


class AnalyzeReference(show.ShowOne):
    """Analyze reference"""

    log = logging.getLogger(__name__ + '.AnalyzeReference')

    def get_parser(self, prog_name):
        parser = super(AnalyzeReference, self).get_parser(prog_name)
        parser.add_argument(
            'reference',
            metavar='<reference>',
            help='Reference to analyze')
        return parser

    def take_action(self, parsed_args):
        self.log.debug('take_action(%s)', parsed_args)

        account = self.app.client_manager.get_account()
        reference = parsed_args.reference

        data = self.app.client_manager.directory.get(
            account, reference)

        info = {'account': account,
                'name': reference,
                'meta0': [],
                'meta1': []}
        for d in data['dir']:
            if d['type'] == 'meta0':
                info['meta0'].append(d['host'])
            elif d['type'] == 'meta1':
                info['meta1'].append(d['host'])

        for srv_type in ['meta0', 'meta1']:
            info[srv_type] = ', '.join(h for h in info[srv_type])

        return zip(*sorted(info.iteritems()))
