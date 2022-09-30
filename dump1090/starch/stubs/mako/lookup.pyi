# -*- python -*-

# typing stubs for mako

from mako.template import Template
from typing import List, Optional

class TemplateCollection(object):
    def get_template(self, uri: str, relativeto: Optional[str] = None) -> Template: ...

class TemplateLookup(TemplateCollection):
    def __init__(self,directories: Optional[List[str]] = None,
                 module_directory: Optional[str] = None,
                 imports: Optional[List[str]] = None): ...


