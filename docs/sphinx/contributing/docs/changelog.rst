.. _contrib-changelog:

=================
Changelog updates
=================
This page shows upstream Pigweed maintainers how to update the Pigweed
changelog.

.. _contrib-changelog-quickstart:

----------
Quickstart
----------
#. Get a `Gemini API key <https://ai.google.dev/gemini-api/docs/api-key>`_.

   .. note::

      Googlers should follow their team's process for getting an API key.

#. Export ``GEMINI_API_KEY`` as an OS environment variable.

#. Run the changelog tool:

   .. code-block:: console

      ./pw changelog --year=<YYYY> --month=<MM>

   Replace ``<YYYY>`` with a valid year and ``<MM>`` with a valid month.

   This step will take 15 to 30 minutes to complete.

#. Fix formatting errors:

   .. code-block:: console

      ./pw format

#. Preview the docs and verify that the new content is being rendered correctly:

   .. code-block:: console

      bazelisk run //docs:serve

#. Push the change up to Gerrit.

#. Review the generated content and make sure it's accurate.

#. For each story mentioned in the changelog update, tag a relevant owner
   for review. E.g. if a story revolves around ``pw_kernel``, tag the
   ``pw_kernel`` owners to review that story.

.. _contrib-changelog-modify:

-------------------------------------
Fixing issues in the generated output
-------------------------------------
If there are problems with the generated output, ideally we fix the problem
in the changelog tool's logic rather than by making one-off edits in the newly
generated doc. That way, the fix carries over to all subsequent generated docs.
See :ref:`contrib-changelog-impl`.

If it's infeasible for you to fix the issue in the changelog tool, it's OK to
modify the generated doc directly. Please leave a comment on :bug:`447633516`
explaining what issue you encountered and how you manually fixed it.

.. _contrib-changelog-impl:

--------------
Implementation
--------------
This section is for upstream Pigweed maintainers who need to fix a bug in
the changelog tool or want to modify how it works.

Goals
=====
Please understand the project goals before modifying its implementation:

* **100% automation**. Any Pigweed maintainer should be able to run a single
  command and get back a changelog update that is ready to be published
  without any manual editing. The new changelog update should get automatically
  glued into the docs build system.

  .. note::

     This is v3 of Pigweed's changelog automation. In v1 and v2 we learned
     that even 80% automation is not enough. The only way we can keep up with
     publishing monthly changelog updates is to make it as easy as running a
     single command.

* **Stories, not commits**. Related commits should be grouped into "stories"
  that summarize the larger body of work. E.g. if there are 6 commits related
  to enabling IPC in ``pw_kernel``, there should be one story along the lines
  of ``pw_kernel: Initial IPC support`` and that story should link to the 6
  related commits.

* **User-facing stories only**. If it's not relevant to a downstream project,
  it doesn't belong in the changelog.

* **Interesting, not comprehensive**. Our readers are not interested in a
  comprehensive digest of how Pigweed has changed over the given timeframe. For
  that, they can read the commit log. The purpose of the changelog is to spread
  awareness that Pigweed is continuing to evolve and to highlight the most
  impactful larger bodies of work that are happening.

* **Inverted pyramid**. Following the `inverted pyramid`_ principle from
  journalism, the most important information (i.e. the most newsworthy stories)
  should be shown first.

Architecture
============
The changelog automation uses GenAI, but it's not an agent architecture.
It's a Python script (:cs:`docs/sphinx/changelog/py/changelog.py`) that
orchestrates a precise series of functional-programming-style transformations
over the commits that merged in upstream Pigweed within the specified 1-month
timeframe.

Some of the steps don't involve GenAI. E.g. gathering the commits is handled
via Python and the Git CLI.

The only way that the changelog automation uses GenAI is through the Gemini
API. E.g. to determine if a commit is "newsworthy" or not, we provide the
data for a specific commit (the message and diff) along with a set of criteria
around what we consider "newsworthy"
(:cs:`docs/sphinx/changelog/py/templates/newsworthy.tmpl`), and then we prompt
Gemini API to output a boolean flag indicating whether the commit meets our
"newsworthy" criteria or not. We always use structured output.

In general, we parallelize the Gemini API invocations as much as possible.
E.g. the "newsworthy" step described in the last paragraph can be run in
parallel, because Gemini API looks at each commit individually.

Lifecycle
=========
Here's the series of transformations that
:cs:`docs/sphinx/changelog/py/changelog.py` performs:

#. Gather all commits that occurred within the specified month. This is
   done by parsing Git commit data with Python.

#. Remove reverted commits. This is also done with only Python.

#. Determine "newsworthy" commits. For each commit, we provide commit data (the
   commit message and diff) along with a set of criteria
   (:cs:`docs/sphinx/changelog/py/templates/newsworthy.tmpl`)
   explaining what's newsworthy. Gemini provides a boolean yes or no to the
   question of whether the commit is newsworthy. We also generate a summary of
   each commit at this step.

#. Try to infer a "parent" for each commit. We provide Gemini a target commit,
   along with all the commits that merged before it that month, and prompt
   Gemini to identify the most relevant "ancestor" commit. (It's possible that
   there is no relevant ancestor.)

   Template: :cs:`docs/sphinx/changelog/py/templates/group.tmpl`

   .. note::

      If all commits were associated with issues, we wouldn't need to
      involve Gemini at this step.

#. Group the commits into related "stories" based on the parent data from the
   previous step. E.g. all commits related to enabling IPC in ``pw_kernel``
   are grouped into a single story. We do this by following the chain of
   parent/child connections that were established in the last step. E.g. if
   commit A is the parent of commit B, and commit B is the parent of commit C,
   then we group A, B, and C into a story.

   Template: :cs:`docs/sphinx/changelog/py/templates/group.tmpl`

#. Draft content for each story.

   Template: :cs:`docs/sphinx/changelog/py/templates/draft.tmpl`

#. Copyedit the drafts (use reST formatting, remove links, etc.).

   Template: :cs:`docs/sphinx/changelog/py/templates/copyedit.tmpl`

#. Sort the stories in order of importance.

   Template: :cs:`docs/sphinx/changelog/py/templates/sort.tmpl`

#. Determine which 15 stories are most interesting and drop the rest with
   Gemini.

   Template: :cs:`docs/sphinx/changelog/py/templates/drop.tmpl`

#. Transform the stories data into a reStructuredText (reST) document.

#. Glue the new reST file into the docs system.

.. _inverted pyramid: https://en.wikipedia.org/wiki/Inverted_pyramid_(journalism)
