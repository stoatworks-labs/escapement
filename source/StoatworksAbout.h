/*
 * Stoatworks Labs - About window data for Escapement.
 *
 * HAND-WRITTEN placeholder, 2026-08-26. Escapement has no entry in the website's
 * projects.json yet, so sync-about.py cannot generate this file and the URLs
 * below are the ones it WILL generate once it can. Add the project there and
 * re-run the sync before the first release, or the About block ships pointing
 * at four pages that do not exist.
 *
 * `version` here is a fallback read from this repo's own manifest at sync
 * time. Anything with a build step injects the real one at build time and
 * overrides this.
 */
#pragma once

namespace stoatworks::about
{
    inline constexpr auto name = "Escapement";
    inline constexpr auto slug = "escapement";
    inline constexpr auto hook = "A video feedback rig, and the fractals it settles into";
    inline constexpr auto licence = "MIT";
    inline constexpr auto guide = "https://stoatworks-labs.com/software/escapement/guide/";
    inline constexpr auto page = "https://stoatworks-labs.com/software/escapement/";
    inline constexpr auto repo = "https://github.com/stoatworks-labs/escapement";
    inline constexpr auto versionFallback = "v0.1.0";

    inline constexpr auto org = "Stoatworks Labs";
    inline constexpr auto home = "https://stoatworks-labs.com";
    inline constexpr auto tagline = "Open tools for the people who run the show.";

    /* The canonical funding links, matching FUNDING.yml and the support footer. */
    struct Link { const char* name; const char* url; };
    inline constexpr Link funding[] = {
        { "GitHub Sponsors", "https://github.com/sponsors/stoatworks-labs" },
        { "Ko-fi", "https://ko-fi.com/stoatworkslabs" },
        { "Patreon", "https://patreon.com/StoatworksLabs" },
        { "Liberapay", "https://liberapay.com/stoatworks-labs" },
    };
}
