import re

# Utility PCBVersion data class which parses a version string into major, minor and patch integers
class PCBVersion:
    major: int = 0
    minor: int = 0
    patch: int = 0
    fullVersion: str = None
    fullString: str = None

    def __init__(self, ctx=None, fullString: str = None) -> None:
        if fullString is not None:
            self.fullString = fullString
            splittedStr = fullString.split('.')
            self.major = int(splittedStr[0].replace('v', ''))
            self.minor = int(splittedStr[1]) if len(splittedStr) > 1 else 0
            self.patch = int(splittedStr[2]) if len(splittedStr) > 2 else 0
            self.fullVersion = re.sub('[a-zA-Z]', '', fullString)
        else:
            self.major = ctx["major"]
            self.minor = ctx["minor"]
            self.patch = ctx["patch"]
            self.fullVersion = f"{ctx['major']}.{ctx['minor']}.{ctx['patch']}"
            self.fullString = f"{ctx['major']}.{ctx['minor']}.{ctx['patch']}"

    
    # Compare whether two versions are the same. If self is ahead of alt, will return a positive 
    # number. If it is the same, will return 0. If self is behind alt, will return a negative number
    def compare(self, alt: "PCBVersion"):
        if self.major == alt.major:
            if self.minor == alt.minor:
                if self.patch == alt.patch:
                    return 0
                else:
                    return self.patch - alt.patch
            else:
                return self.minor - alt.minor
        else:
            return self.major - alt.major