# Copyright 2023 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Install and check status of CHRE."""

import pathlib
from typing import Sequence

import pw_package.git_repo
import pw_package.package_manager


class Chre(pw_package.git_repo.GitRepo):
    """Install and check status of CHRE."""

    def __init__(self, *args, **kwargs):
        # android16-qpr2-release
        super().__init__(
            *args,
            name='chre',
            url='https://android.googlesource.com/platform/system/chre',
            commit='8845bc1271be36fc5e5ebb7d48b88441bbf96dcc',
            **kwargs,
        )

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (
            f'{self.name} installed in: {path}',
            "Enable by running 'gn args out' and adding this line:",
            f'  dir_pw_third_party_chre = "{path}"',
        )


pw_package.package_manager.register(Chre)
