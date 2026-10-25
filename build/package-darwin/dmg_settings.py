# dmgbuild settings for the openMSX disk image, used by app.mk.
# Usage: dmgbuild -s dmg_settings.py -D app=<openMSX.app> -D background=<png> \
#            <volume name> <output.dmg>
# dmgbuild writes the Finder layout (.DS_Store) itself, so the volume name can
# change freely between builds.

import os.path

app = defines['app']
appName = os.path.basename(app)

format = 'UDZO'
compression_level = 9

files = [app]
symlinks = {'Applications': '/Applications'}
hide_extensions = [appName]

# dmg_bg.tiff holds dmg_bg.png and dmg_bg@2x.png as one multi-resolution image,
# so the background stays sharp on Retina displays. It is JPEG-compressed, which
# makes it about 5x smaller than the LZW TIFF dmgbuild would make from the PNGs.
# Regenerate it after changing either PNG, from this directory, with:
#   magick dmg_bg.png dmg_bg@2x.png -alpha off -units PixelsPerInch \
#       -set density '%[fx:t==0?72:144]' -compress jpeg -quality 90 dmg_bg.tiff
background = defines['background']

# The window is sized to dmg_bg.png (600x403) so no blank area shows beside it.
window_rect = ((400, 100), (600, 403))
icon_size = 96
text_size = 12

# The x values are the centres of the two boxes drawn in dmg_bg.png.
# The y is not the box centre: Finder draws the label below the icon, so the
# icon+label block is centred instead. Measured from a rendered DMG, the block
# spans -38 (top of the icon tile) to +71 (bottom of the label, descenders
# included) relative to the position, i.e. 109px tall inside a 142px box, so
# centring leaves ~17px above and below.
icon_locations = {
	appName: (156, 185),       # box spans x 64-248, y 130-272
	'Applications': (443, 185), # box spans x 351-535, y 130-272
	# Out of view for users that have Finder show hidden files.
	'.background.tiff': (2000, 2000),
}
