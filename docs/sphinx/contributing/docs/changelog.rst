.. _contrib-changelog:

================================
Changelog guide for contributors
================================
This page shows upstream Pigweed maintainers how to update the changelog and
understand how the changelog automation works.

.. _contrib-changelog-quickstart:

----------
Quickstart
----------
#. Open an agent product that supports skills, e.g. Antigravity.

#. Start the changelog automation by providing a prompt like this:

   .. code-block:: none

      Create a changelog update for March 2026.

.. _contrib-changelog-theory:

-------------------
Theory of operation
-------------------
The changelog automation follows an iterative process to transform a large number
of raw git commits into a curated list of "stories" that are meaningful to
downstream projects.

#. **Commit ingestion**: The agent runs a script that returns commit data, typically
   in batches of 25.

#. **Story aggregation**: The agent groups related commits into a single "story"
   with a title, body, one-line highlight, and score (representing user-facing impact).
   The agent does all of its work in a TOML file. This TOML file is a temporary artifact.

#. **Iterative refinement**: Similar stories are merged, overly broad ones are split.

#. **Validation**: When the agent attempts to fetch the next batch of commits,
   the ``next`` script that the agent invokes validates the WIP data and refuses to
   yield the next batch until the agent fixes the issues that the script has detected.

#. **Transformation**: The agent invokes the ``end`` script, and this script transforms
   the TOML data into reStructuredText.

All relevant source code can be found in the following directories:

* :cs:`.agents/skills/changelog/`
* :cs:`docs/sphinx/changelog/`
