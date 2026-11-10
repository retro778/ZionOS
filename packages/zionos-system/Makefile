# Builds generated files for the zionos-system packages.
VERSION       ?= $(shell dpkg-parsechangelog -S Version | sed 's/[-+~].*//')
VERSION_SHORT := $(shell echo $(VERSION) | cut -d. -f1-2)
CODENAME      ?= Aleph
CODENAME_LOWER:= $(shell echo $(CODENAME) | tr A-Z a-z)
DEBIAN_SUITE  ?= trixie
GEN := gen

SUBST = sed -e 's/@VERSION@/$(VERSION)/g' -e 's/@VERSION_SHORT@/$(VERSION_SHORT)/g' \
            -e 's/@CODENAME@/$(CODENAME)/g' -e 's/@CODENAME_LOWER@/$(CODENAME_LOWER)/g' \
            -e 's/@DEBIAN_SUITE@/$(DEBIAN_SUITE)/g'

all: $(GEN)/os-release $(GEN)/issue $(GEN)/issue.net images

$(GEN):
	mkdir -p $(GEN)

$(GEN)/os-release: base/os-release.in debian/changelog | $(GEN)
	$(SUBST) $< > $@

$(GEN)/issue: debian/changelog | $(GEN)
	printf 'ZionOS $(VERSION_SHORT) ($(CODENAME)) \\n \\l\n\n' > $@

$(GEN)/issue.net: debian/changelog | $(GEN)
	printf 'ZionOS $(VERSION_SHORT) ($(CODENAME))\n' > $@

images: | $(GEN)
	rsvg-convert -w 1920 -h 1080 artwork/zionos-wallpaper.svg -o $(GEN)/zionos-wallpaper-1920x1080.png
	rsvg-convert -w 3840 -h 2160 artwork/zionos-wallpaper.svg -o $(GEN)/zionos-wallpaper-3840x2160.png
	rsvg-convert -w 1024 -h 768 artwork/zionos-boot.svg -o $(GEN)/zionos-boot.png
	mkdir -p $(GEN)/calamares
	rsvg-convert -w 256 -h 256 artwork/zionos-logo.svg -o $(GEN)/calamares/zionos-logo.png
	rsvg-convert -w 900 -h 500 artwork/zionos-installer.svg -o $(GEN)/calamares/welcome.png
	rsvg-convert -w 900 -h 500 artwork/zionos-installer.svg -o $(GEN)/calamares/slide1.png
	for f in calamares/branding/zionos/branding.desc; do mkdir -p $(GEN)/$$(dirname $$f); $(SUBST) $$f > $(GEN)/$$f; done
	for s in 16 22 24 32 48 64 128 256; do \
	  rsvg-convert -w $$s -h $$s artwork/zionos-logo.svg -o $(GEN)/zionos-logo-$$s.png; done

clean:
	rm -rf $(GEN)

.PHONY: all images clean
