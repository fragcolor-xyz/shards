---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Contributing changes

Fragcolor documentation exists under the [/docs](https://github.com/fragcolor-xyz/shards/tree/devel/docs) folder of the Shards repository.

This guide will get you started with contributing documentation changes to Fragcolor projects. We'll use the GitHub Desktop (GD) to deal with git as GD is easier to use than the git command line.

Also, all these steps are valid for code contributions too.

*For comprehensive coverage of git/ GitHub workflow, check out GitHub's excellent [Quickstart Tutorial](https://docs.github.com/en/get-started/quickstart/hello-world).*

## Clone the Repo

To begin, you'll need a copy of the project's repository on your local machine.

Go to the repository's GitHub page. Click the 'Code' button on the right, select HTTPS (under the heading 'Clone'), and copy the git repository path.

![Copy repo URL to clone](assets/contrib_clone-copy.png)

Open GD and choose the option 'Clone a repository from the Internet...'

![Select options to clone](assets/contrib_clone-gd.png)

Choose the 3rd tab (URL) on the window that opens, and paste the copied git repository URL in the first input box.

*The second input box is the local path where git will clone your repository. You can change this as desired.*

![Paste repo URL to clone](assets/contrib_clone-paste.png)

Cloning might take a couple of minutes (depending on the size of the repository). After it completes, you'll be able to view the repository files on your local system.

![Repo cloned](assets/contrib_clone-done.png)

## Create a branch

You will need to create a new branch from the main/ default branch to hold your changes.

*Never work on the main branch of the repository; always create a new branch for your changes.*

You can create a new branch using GD. Click the 'Current Branch' dropdown to show the available branches for this repository. Now click on 'New Branch' button on the right-hand side.

![Show new branch options](assets/contrib_branch-start.png)

A modal will pop up asking for the new branch's name. This modal also explains on what branch is this new branch based. Ensure that the source branch is the default branch of the repository. In this case, it's 'develop'.

Click on 'Create Branch'.

![Create new branch](assets/contrib_branch-name.png)

Publish this local branch to the project's remote (from where we cloned the repository).

![publish new branch](assets/contrib_branch-pub.png)

If you don't have write access to a project repository, you can't push your changes to it. So, GD will ask you to fork (create a copy of) the target repository instead. Forking will put a copy of the original repository in your GitHub account. You can now push your local branch/ changes to this fork repository in your GitHub account.
Fork the repository.

![Fork repository](assets/contrib_branch-fork.png)

After forking, choose 'To contribute to the parent project' option.

![Continue after forking](assets/contrib_branch-fork-1.png)

Publish the new branch to your fork repository.

![Publish branch after forking](assets/contrib_branch-pub-1.png)

On GD, you will now be able to see uncommitted changes (if any) in your local branch.

![See uncommitted changes](assets/contrib_commit-ready.png)

## Make & test changes

*Run all terminal commands from the project folder that contains `MkDocs.yml`. For the Shards repository, this folder is `...shards/docs`).*

MkDocs can build and serve the documentation website locally while you're making changes. The served pages reflect your documentation changes in real-time (also called hot reloading).

Before starting the MkDocs live-preview server, we need to install the required plugins.

MkDocs plugins get registered in the `MkDocs.yml` file against the keyword 'plugins'. Here, we have only one plugin: [awesome-pages](https://github.com/lukasgeiter/mkdocs-awesome-pages-plugin).

![Search for plugins in MkDocs.yml](assets/changes_find-plugins.png)

Install MkDocs plugins using the `pip install mkdocs-awesome-pages-plugin` command from the terminal.

=== "Command"

    ![Install MkDocs plugins](assets/changes_install-plugins.png)

Start the MkDocs live-preview server with the `mkdocs serve` command. Once the server is running, the terminal will display the locally served website's URL path.

=== "Command"

    ![Start MkDocs live-preview server](assets/changes_serve.png)

=== "Result"

    ![Live-preview server started](assets/changes_served.png)

Navigate to this path in your local browser to access the served site.

![Documentation site served](assets/changes_site.png)

Now we're ready to make some changes!

As an exercise, let's change the case of the header's title text. Change 'Fragcolor documentation' (sentence case) to 'Fragcolor Documentation' (title case). Save the edit.

Before the change:
![Edit the source file](assets/changes_change.png)

After the change:
![Source file edited](assets/changes_changed.png)

And now, let's go to our local URL to preview this change.

![Change previewed live](assets/changes_previewed.png)

## Commit & push changes

GD will now show a summary of changes in this new local branch.

*Pink color highlights (marked with `-`) denote deleted lines. Blue color highlights (marked with `+`) denote added lines.*

![See change summary](assets/commit-push_changes.png)

Commit (save) your changes to the local repository branch.

To commit, click the 'Commit to LOCAL-REPO-NAME' in the bottom left-hand corner of GD.

![Commit changes to local](assets/commit-push_commit.png)

GD will ask you to add a summary (commit message) and a description of the changes.

Add the commit message summary/ description and click on the 'Commit to ...' button below these fields. This commit saves the changes to your new branch locally (in your fork).

Now, click on the 'Push origin' button on the right-hand side. This action pushes your branch's local commits to your fork on GitHub (i.e., the origin remote).

![Add commit message and push changes to remote origin](assets/commit-push_push.png)

At this point, GD will show that no local changes exist. This is because there are no outstanding changes on our local that need to be pushed to remote origin.

GD will now prompt you to raise a Pull Request.

![Commit pushed to remote origin; raise PR via GitHub Desktop](assets/commit-push_pushed-gd.png)

You'll see the same message/ prompt on the GitHub page of your (forked) remote origin repository.

![Commit pushed to remote origin; raise PR via GitHub](assets/commit-push_pushed-github.png)

You can start the process of raising a PR from either GD or GitHub website. In either case, you'll end up on the GitHub website's PR page, as explained in the next section.

## Raise a pull request

A Pull Request (or PR) merges/ combines the changes of the PR branch into the target (main) branch.

*PRs may be raised between branches of the same repository from a fork to the original repository. Here we are dealing with the latter.*

Raise a PR by clicking 'Create Pull Request' in GD. Or, you can click 'Compare & pull request' on your branch/ repository's GitHub page. In both cases, you'll end up on the 'Open a pull request' page on GitHub.

Fill in the PR title and message. Then click the 'Create pull request' button on the bottom right-hand corner. This action will create a Pull Request and route it to a reviewer.

![Raise PR via GitHub](assets/pr_raise.png)

Every PR gets a unique URL. This URL tracks the review discussion between the contributor and the reviewer(s). It also has details like files changed, commit messages, etc.

![PR raised](assets/pr_raised.png)

Every PR gets routed to a reviewer or a maintainer (of that repository) for a review. If the reviewer agrees with the changes, they'll approve and merge the PR into the target branch. If they need further changes, they will discuss via the comments section in the PR.

*It's a good practice to delete your branches for merged/ closed PRs.*


--8<-- "includes/license.md"
