#pragma once

#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <mutex>
#include <optional>

#ifndef GATEDADDY_REPO
 #define GATEDADDY_REPO "neomoney333/gate-daddy"
#endif

namespace gd
{

struct UpdateInfo
{
    juce::String version, pageUrl, downloadUrl;
};

// Compares dotted versions: isNewer ("1.2.0", "1.1.9") == true
inline bool isNewerVersion (const juce::String& latest, const juce::String& current)
{
    auto a = juce::StringArray::fromTokens (latest.trimCharactersAtStart ("vV"), ".", "");
    auto b = juce::StringArray::fromTokens (current.trimCharactersAtStart ("vV"), ".", "");
    for (int i = 0; i < juce::jmax (a.size(), b.size()); ++i)
    {
        const int x = a[i].getIntValue(), y = b[i].getIntValue();
        if (x != y)
            return x > y;
    }
    return false;
}

// Checks GitHub Releases once per host session, on a background thread.
// Held by editors through a SharedResourcePointer, so the thread is always
// stopped before the plugin binary can be unloaded.
class UpdateService : private juce::Thread
{
public:
    UpdateService() : juce::Thread ("Gate Daddy Update Check")
    {
        auto& st = state();
        const std::lock_guard<std::mutex> lock (st.mutex);
        if (! st.started)
        {
            st.started = true;
            startThread (juce::Thread::Priority::low);
        }
    }

    ~UpdateService() override { stopThread (8000); }

    static std::optional<UpdateInfo> available()
    {
        auto& st = state();
        const std::lock_guard<std::mutex> lock (st.mutex);
        return st.result;
    }

private:
    struct State
    {
        std::mutex mutex;
        bool started = false;
        std::optional<UpdateInfo> result;
    };

    static State& state()
    {
        static State s;
        return s;
    }

    void run() override
    {
        const juce::URL url ("https://api.github.com/repos/" GATEDADDY_REPO "/releases/latest");
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                 .withConnectionTimeoutMs (6000)
                                                 .withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: GateDaddy"));
        if (stream == nullptr || threadShouldExit())
            return;

        const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
        const auto tag = json["tag_name"].toString();
        if (tag.isEmpty() || ! isNewerVersion (tag, JucePlugin_VersionString))
            return;

        UpdateInfo info { tag.trimCharactersAtStart ("vV"), json["html_url"].toString(), {} };
       #if JUCE_MAC
        const juce::String wanted = ".pkg";
       #else
        const juce::String wanted = "windows";
       #endif
        if (auto* assets = json["assets"].getArray())
            for (auto& a : *assets)
                if (a["name"].toString().toLowerCase().contains (wanted))
                    info.downloadUrl = a["browser_download_url"].toString();
        if (info.downloadUrl.isEmpty())
            info.downloadUrl = info.pageUrl;

        auto& st = state();
        const std::lock_guard<std::mutex> lock (st.mutex);
        st.result = info;
    }
};

} // namespace gd
