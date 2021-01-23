#include <iostream>
#include <memory>
#include <git2.h>
#include <map>
#include <unordered_set>
#include <ctime>
#include <args.hxx>
#include <utility>

using namespace std;

typedef unsigned int ui;

class LineStats {
public:
    string author;
    ui commits, linesAdded, linesRemoved;

    explicit LineStats(string &author) {
        this->author = author;
        commits = linesAdded = linesRemoved = 0;
    }
};

class DateLineStats {
public:
    string date;
    map<string, LineStats> lineStats;

    explicit DateLineStats(string &date) {
        this->date = date;
    }
};

class CommitEntry {
public:
    string commitId;
    string author;
    git_time_t time;
    git_commit *commit;

    CommitEntry(string commitId, string author, git_time_t time, git_commit *commit) : commitId(std::move(commitId)),
                                                                                                     author(std::move(author)),
                                                                                                     time(time),
                                                                                                     commit(commit) {}

    bool operator==(const CommitEntry &rhs) const {
        return commitId == rhs.commitId || (
               author == rhs.author &&
               time == rhs.time);
    }

    bool operator!=(const CommitEntry &rhs) const {
        return !(rhs == *this);
    }
};

struct CommitEntryHash{
    size_t operator()(const CommitEntry &e) const {
        std::hash<string> hashString;
        std::hash<git_time_t> hashTime;
        return hashString(e.commitId) ^ hashString(e.author) ^ hashTime(e.time);
    }
};

typedef unordered_set<CommitEntry, CommitEntryHash> CommitSet;

class RepoStats {
private:
    map<string,DateLineStats> dateLineStatsMap;

public:
    void run(const string &path, ui days) {
        git_libgit2_init();

        git_repository *repo;
        if(git_repository_open(&repo, path.c_str()) != 0) {
            throw runtime_error("Could not find repository");
        }

        auto commitSet = RepoStats::findCommits(repo, days);
        for(const CommitEntry &entry: *commitSet) {

            git_commit *commit = entry.commit;
            string message(git_commit_message(commit));

            if (git_commit_parentcount(commit) != 1) {
                //cout << "Skipping commit " << git_commit_summary(commit) << endl;
                continue;
            }

            git_commit *parent_commit;
            git_commit_parent(&parent_commit, commit, 0);

            string commitDate = RepoStats::commitDate(commit);
            string authorName(entry.author);

            DateLineStats *dateLineStats = &(this->dateLineStatsMap.emplace(commitDate,
                                                                            DateLineStats(commitDate)).first->second);
            LineStats *ls = &dateLineStats->lineStats.emplace(authorName, LineStats(authorName)).first->second;

            RepoStats::updateStats(repo, commit, parent_commit, ls);

        }

        git_repository_free(repo);
        git_libgit2_shutdown();

        this->renderTable();
    }

    void renderTable() {
        for(auto &dl: dateLineStatsMap) {
            cout << dl.second.date << ":" << endl;
            for (auto &p: dl.second.lineStats) {
                auto stats = p.second;
                cout << "\t" << stats.author << ":" << endl
                    << "\t\tCommits: " << stats.commits << endl
                    << "\t\tAdded lines : " << stats.linesAdded << endl
                    << "\t\tRemoved lines : " << stats.linesRemoved << endl;
            }
        }
    }

    static unique_ptr<CommitSet> findCommits(git_repository *repo, ui nDays) {
        auto commits = make_unique<CommitSet>();

        git_branch_iterator *branchIterator;
        git_branch_iterator_new(&branchIterator, repo, GIT_BRANCH_ALL);

        git_reference *branch;
        git_branch_t type;
        git_time_t latestCommitTime(0);
        vector<git_commit*> commitList;

        while(git_branch_next(&branch, &type, branchIterator) == 0) {
            const char* branchName;
            git_branch_name(&branchName, branch);

            //cout << "Scanning branch " << branchName << endl;

            git_reference_resolve(&branch, branch);
            const git_oid *headId = git_reference_target(branch);

            git_revwalk *walk;
            if(git_revwalk_new(&walk, repo) != 0) {
                throw runtime_error("Could not initialize walker");
            }

            if(git_revwalk_push(walk, headId) != 0) {
                throw runtime_error("Could not iterate branch");
            }

            git_oid commitId;

            while (git_revwalk_next(&commitId, walk) == 0) {
                git_commit *commit;
                if (git_commit_lookup(&commit, repo, &commitId) != 0) {
                    throw runtime_error("Could not read commit");
                }

                git_time_t commitTime = git_commit_time(commit);
                if(commitTime > latestCommitTime) {
                    latestCommitTime = commitTime;
                }

                commitList.push_back(commit);
            }

            git_revwalk_free(walk);
        }

        for(git_commit *commit: commitList) {
            git_time_t commitTime = git_commit_time(commit);
            if (latestCommitTime - commitTime <= nDays*24*3600) {
                const git_oid *commitId = git_commit_id(commit);
                const git_signature *author = git_commit_author(commit);
                string authorName(author->name);

                commits->emplace(CommitEntry(
                        string(git_oid_tostr_s(commitId)),
                        authorName,
                        git_commit_time(commit),
                        commit
                        ));
            }
        }

        return commits;
    }

    static string commitDate(git_commit *commit) {
        time_t t;
        char out[32];
        git_time_t commitTime = git_commit_time(commit);

        t = (time_t)commitTime;
        strftime(out, sizeof(out), "%Y/%m/%d", gmtime(&t));
        return string(out);
    }

    static void updateStats(git_repository *repo, git_commit *commit, git_commit *parent_commit, LineStats *ls) {
        git_tree *commit_tree;
        git_tree *parent_tree;
        git_diff *diff;
        git_diff_stats *stats;

        git_commit_tree(&commit_tree, commit);
        git_commit_tree(&parent_tree, parent_commit);

        git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, nullptr);
        git_diff_get_stats(&stats, diff);

        ++(ls->commits);
        ls->linesAdded += git_diff_stats_insertions(stats);
        ls->linesRemoved += git_diff_stats_deletions(stats);
    }
};

int main(int argc, char **argv) {
    args::ArgumentParser parser("Prints daily code line stats");
    args::Positional<std::string> path(parser, "path", "Path to repo");
    args::Positional<int> days(parser, "days", "Number of trailing days to print");

    try
    {
        parser.ParseCLI(argc, argv);
    } catch (exception &err) {
        std::cout << parser;
        return 1;
    }

    RepoStats repoStats;

    try {
        repoStats.run(args::get(path), args::get(days));
    } catch (exception &err) {
        cout << err.what();
        return 1;
    }

    return 0;
}
