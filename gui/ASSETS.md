# GUI assets

The 3D models, textures, sounds and fonts used by the GUI (about 1.5 GB) are not stored
in git. They are published as an archive attached to the GitHub release `assets-v1`:

```bash
./gui/scripts/fetch_assets.sh     # downloads, checks the SHA-256 and extracts into gui/assets/
```

## Credits

Third-party models, used under the Creative Commons Attribution 4.0 license
(https://creativecommons.org/licenses/by/4.0/):

| Asset | Folder | Author | Source |
| --- | --- | --- | --- |
| Moon | `models/moon/` | Nestaeric | https://sketchfab.com/3d-models/moon-e6d36bb905ed42049a234e7be571d8a3 |
| Phoenix bird | `screen/phoenix/` | NORBERTO-3D | https://sketchfab.com/3d-models/phoenix-bird-844ba0cf144a413ea92c779f18912042 |
| Mew - Flying | `characters/mew/` | Marco Fascinetto | https://sketchfab.com/3d-models/mew-flying-5c3814b08f48493396f298d48629d4dc |

Fan models of Pokémon characters:

| Asset | Folder | Source |
| --- | --- | --- |
| Mew | `characters/mew/` | see above |
| Charizard | `characters/charizard/` | source to be confirmed |

Pokémon, Mew and Charizard are trademarks of Nintendo, Creatures Inc. and GAME FREAK inc.
This non-commercial student project is not affiliated with or endorsed by them; the
models are fan creations credited to their authors.

Third-party fonts, source to be confirmed: `fonts/ENDOR.ttf`, `fonts/GAMEDAY.ttf`,
`fonts/GamersDemo*.ttf`.

Everything else (player characters, wildlife, props, resources, islands, sounds, logo,
loading screen) was made by the project team.
