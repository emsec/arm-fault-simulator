#pragma once

#include "m-ulator/types.h"

#include <functional>

namespace mulator
{
    template<typename... Args>
    class CallbackHook
    {
    public:
        using FuncPtr = void (*)(Args..., void*);
        using FuncStd = std::function<void(Args..., void*)>;

        template<typename Func>
        u32 add(const Func& func, void* user_data = nullptr)
        {
            auto id = m_next_id++;
            m_callbacks.emplace_back(id, func, user_data);
            return id;
        }

        void execute(Args... args) const
        {
            if (!empty())
            {
                for (const auto& entry : m_callbacks)
                {
                    if (entry.to_remove)
                    {
                        continue;
                    }

                    if (entry.f_ptr != nullptr)
                    {
                        entry.f_ptr(std::forward<Args>(args)..., entry.user_data);
                    }
                    else
                    {
                        entry.f_std(std::forward<Args>(args)..., entry.user_data);
                    }
                }
                cleanup_removed();
            }
        }

        void remove(u32 id)
        {
            for (auto& entry : m_callbacks)
            {
                if (entry.id == id)
                {
                    m_remove_requested = true;
                    entry.to_remove    = true;
                    return;
                }
            }
        }

        void clear()
        {
            m_remove_requested = true;
            for (auto& entry : m_callbacks)
            {
                entry.to_remove = true;
            }
        }

        bool empty() const
        {
            return m_callbacks.empty();
        }

    private:
        struct Entry
        {
            u32 id;
            FuncPtr f_ptr = nullptr;
            FuncStd f_std = {};
            void* user_data;
            bool to_remove = false;

            Entry(u32 id, FuncPtr f_ptr, void* user_data) : id(id), f_ptr(f_ptr), user_data(user_data){};
            Entry(u32 id, FuncStd f_std, void* user_data) : id(id), f_std(f_std), user_data(user_data){};
        };

        void cleanup_removed() const
        {
            if (m_remove_requested)
            {
                m_callbacks.erase(std::remove_if(m_callbacks.begin(), m_callbacks.end(), [](auto& e) { return e.to_remove; }), m_callbacks.end());
                m_remove_requested = false;
            }
        }

        mutable std::vector<Entry> m_callbacks;
        mutable bool m_remove_requested = false;
        u32 m_next_id                   = 0;
    };

}    // namespace mulator
