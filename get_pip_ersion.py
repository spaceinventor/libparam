#!/usr/bin/env python3
""" Print the Pip compliant semantic version of the Git tag, which is without the 'v' """

from __future__ import annotations

import os
from datetime import datetime
from posixpath import dirname
from subprocess import PIPE, run


def main() -> None:

    # Building with PIP/meson-python, use .dev0 as dirty watermark to satisfy PIP semantic versioning.
    if 'PEP517_BUILD_BACKEND' in os.environ:
        pass  # TODO Kevin: We currently only handle PIP versioning. We probably want another semver for normal build

    ersion: str = run(['git', '-C', dirname(__file__), 'describe', '--long', '--always', '--dirty=+'], stdout=PIPE).stdout.decode()
    ersion = ersion.lstrip('vV').strip('\n\t ')

    semantic_ersion = ersion.split('-')[0]

    ersion_has_patch: bool = semantic_ersion.count('.') > 1

    num_commits_ahead = int(ersion.split('-')[1])

    if ersion_has_patch:
        if num_commits_ahead > 0:
            semantic_ersion += f".post{num_commits_ahead}"
    else:
        semantic_ersion += f".{num_commits_ahead}"

    if ersion.endswith('+'):  # Dirty working tree
        now = datetime.now()
        semantic_ersion += f".dev{now.strftime('%H%M')}"

    print(semantic_ersion, end='')

if __name__ == '__main__':
    main()
